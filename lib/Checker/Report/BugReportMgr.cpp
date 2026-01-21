#include "Checker/Report/BugReportMgr.h"
#include <llvm/Support/ManagedStatic.h>
#include <algorithm>
#include <unordered_set>

static llvm::ManagedStatic<BugReportMgr> global_bug_report_mgr;

BugReportMgr& BugReportMgr::get_instance() {
    return *global_bug_report_mgr;
}

BugReportMgr::BugReportMgr() {
}

BugReportMgr::~BugReportMgr() {
    // Clean up all reports
    for (auto& pair : reports) {
        for (BugReport* report : pair.second) {
            delete report;
        }
    }
}

int BugReportMgr::register_bug_type(
    llvm::StringRef ty_name,
    BugDescription::BugImportance importance,
    BugDescription::BugClassification classification,
    llvm::StringRef desc) {
    
    // Check if already registered
    int id = find_bug_type(ty_name);
    if (id != -1) {
        return id;
    }
    
    // Register new bug type
    id = bug_types.size();
    bug_types.push_back(BugType(id, ty_name, importance, classification, desc));
    bug_type_names[ty_name] = id;
    
    return id;
}

int BugReportMgr::find_bug_type(llvm::StringRef ty_name) {
    auto it = bug_type_names.find(ty_name);
    return (it == bug_type_names.end()) ? -1 : it->second;
}

const BugReportMgr::BugType& BugReportMgr::get_bug_type_info(int ty_id) const {
    assert(ty_id >= 0 && ty_id < (int)bug_types.size() && "Invalid bug type ID");
    return bug_types[ty_id];
}

bool BugReportMgr::insert_report(int ty_id, BugReport* report, bool deduplicate_by_trace) {
    assert(ty_id >= 0 && ty_id < (int)bug_types.size() && "Invalid bug type ID");
    
    if (deduplicate_by_trace && is_duplicate(ty_id, report, deduplicate_by_trace)) {
        // Duplicate found, delete the new report
        delete report;
        return false;
    }
    
    reports[ty_id].push_back(report);
    
    // Track hash for future deduplication
    if (deduplicate_by_trace) {
        size_t hash = report->compute_hash(deduplicate_by_trace);
        report_hashes[hash] = report;
    }
    
    return true;
}

void BugReportMgr::insert_report(int ty_id, BugReport* report) {
    // Default to deduplication enabled (trace-based)
    insert_report(ty_id, report, true);
}

bool BugReportMgr::is_duplicate(int ty_id, const BugReport* report, bool use_trace) const {
    size_t hash = report->compute_hash(use_trace);
    
    // Check if we've seen this hash before
    auto it = report_hashes.find(hash);
    if (it != report_hashes.end()) {
        return true;
    }
    
    // Also check existing reports of the same type
    auto reports_it = reports.find(ty_id);
    if (reports_it != reports.end()) {
        for (const BugReport* existing : reports_it->second) {
            size_t existing_hash = existing->compute_hash(use_trace);
            if (existing_hash == hash) {
                return true;
            }
        }
    }
    
    return false;
}

void BugReportMgr::deduplicate_reports(bool use_trace) {
    // Clear existing hash map
    report_hashes.clear();
    
    // Rebuild with deduplication
    for (auto& pair : reports) {
        int ty_id = pair.first;
        std::vector<BugReport*>& report_list = pair.second;
        
        std::unordered_set<size_t> seen_hashes;
        std::vector<BugReport*> deduplicated;
        
        for (BugReport* report : report_list) {
            size_t hash = report->compute_hash(use_trace);
            
            if (seen_hashes.find(hash) == seen_hashes.end()) {
                seen_hashes.insert(hash);
                deduplicated.push_back(report);
                report_hashes[hash] = report;
            } else {
                // Duplicate found, delete it
                delete report;
            }
        }
        
        report_list = std::move(deduplicated);
    }
}

const std::vector<BugReport*>* BugReportMgr::get_reports_for_type(int ty_id) const {
    auto it = reports.find(ty_id);
    if (it == reports.end()) {
        return nullptr;
    }
    return &it->second;
}

int BugReportMgr::get_src_file_id(llvm::StringRef src_file) {
    auto it = src_file_ids.find(src_file);
    if (it != src_file_ids.end()) {
        return it->second;
    }
    
    int id = src_files.size();
    src_files.push_back(src_file.str());
    src_file_ids[src_file] = id;
    return id;
}

void BugReportMgr::generate_json_report(llvm::raw_ostream& OS, int min_score) const {
    OS << "{\n";
    OS << "  \"TotalBugs\": " << get_total_reports() << ",\n";
    
    // Source files array
    OS << "  \"SrcFiles\": [\n";
    for (size_t i = 0; i < src_files.size(); ++i) {
        OS << "    \"" << src_files[i] << "\"";
        if (i < src_files.size() - 1) OS << ",";
        OS << "\n";
    }
    OS << "  ],\n";
    
    // Bug types and reports
    OS << "  \"BugTypes\": [\n";
    bool first_type = true;
    
    for (size_t ty_id = 0; ty_id < bug_types.size(); ++ty_id) {
        const BugType& bt = bug_types[ty_id];
        const std::vector<BugReport*>* bt_reports = get_reports_for_type(ty_id);
        
        if (!bt_reports || bt_reports->empty()) {
            continue;
        }
        
        // Filter by score
        std::vector<const BugReport*> filtered;
        for (const BugReport* report : *bt_reports) {
            if (report->get_conf_score() >= min_score) {
                filtered.push_back(report);
            }
        }
        
        if (filtered.empty()) {
            continue;
        }
        
        if (!first_type) OS << ",\n";
        first_type = false;
        
        OS << "    {\n";
        OS << "      \"Name\": \"" << bt.bug_name << "\",\n";
        OS << "      \"Description\": \"" << bt.desc << "\",\n";
        OS << "      \"Importance\": \"" << BugDescription::to_string(bt.importance) << "\",\n";
        OS << "      \"Classification\": \"" << BugDescription::to_string(bt.classification) << "\",\n";
        OS << "      \"TotalReports\": " << filtered.size() << ",\n";
        OS << "      \"Reports\": [\n";
        
        for (size_t i = 0; i < filtered.size(); ++i) {
            filtered[i]->export_json(OS);
            if (i < filtered.size() - 1) {
                OS << ",";
            }
            OS << "\n";
        }
        
        OS << "      ]\n";
        OS << "    }";
    }
    
    OS << "\n  ]\n";
    OS << "}\n";
}

void BugReportMgr::print_summary(llvm::raw_ostream& OS) const {
    OS << "\n==================================================\n";
    OS << "               Bug Report Summary\n";
    OS << "==================================================\n\n";
    
    int total = 0;
    
    for (size_t ty_id = 0; ty_id < bug_types.size(); ++ty_id) {
        const BugType& bt = bug_types[ty_id];
        const std::vector<BugReport*>* bt_reports = get_reports_for_type(ty_id);
        
        if (!bt_reports || bt_reports->empty()) {
            continue;
        }
        
        int valid_count = 0;
        for (const BugReport* report : *bt_reports) {
            if (report->is_valid()) {
                valid_count++;
            }
        }
        
        OS << bt.bug_name << " (" << bt.desc << ")\n";
        OS << "  Total: " << bt_reports->size() 
           << " | Valid: " << valid_count << "\n\n";
        
        total += bt_reports->size();
    }
    
    OS << "==================================================\n";
    OS << "Total Bugs Found: " << total << "\n";
    OS << "==================================================\n\n";
}

int BugReportMgr::get_total_reports() const {
    int total = 0;
    for (const auto& pair : reports) {
        total += pair.second.size();
    }
    return total;
}

