#include "Checker/Taint/TaintUtils.h"
#include "Checker/Taint/TaintAnalysis.h"
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/raw_ostream.h>
#include <fstream>
// #include <sstream>

using namespace llvm;
using namespace taint;

// TaintUtils implementation - Essential methods only
std::string TaintUtils::getValueName(const Value* val) {
    if (!val) return "null";
    
    if (val->hasName()) {
        return val->getName().str();
    }
    
    std::string str;
    raw_string_ostream rso(str);
    val->printAsOperand(rso);
    return rso.str();
}

std::string TaintUtils::getInstructionString(const Instruction* inst) {
    if (!inst) return "null";
    
    std::string str;
    raw_string_ostream rso(str);
    rso << inst->getOpcodeName();
    return rso.str();
}

std::string TaintUtils::getFunctionName(const Function* func) {
    if (!func) return "null";
    return func->getName().str();
}

bool TaintUtils::isKnownSourceFunction(const std::string& funcName) {
    static std::set<std::string> sources = {
        "gets", "fgets", "getchar", "scanf", "fscanf",
        "read", "fread", "recv", "recvfrom", "getenv"
    };
    return sources.find(funcName) != sources.end();
}

bool TaintUtils::isKnownSinkFunction(const std::string& funcName) {
    static std::set<std::string> sinks = {
        "system", "exec", "execl", "execle", "execlp",
        "execv", "execve", "execvp", "popen",
        "strcpy", "strcat", "sprintf", "printf", "fprintf",
        "write", "send", "sendto"
    };
    return sinks.find(funcName) != sinks.end();
}

bool TaintUtils::isKnownSanitizerFunction(const std::string& funcName) {
    static std::set<std::string> sanitizers = {
        "strlen", "strnlen", "strncpy", "strncat", "snprintf"
    };
    return sanitizers.find(funcName) != sanitizers.end();
}

void TaintUtils::printTaintValue(const TaintValue* taint, raw_ostream& OS) {
    if (!taint) {
        OS << "null taint";
        return;
    }
    
    OS << "TaintValue {\n";
    OS << "  Value: " << getValueName(taint->value) << "\n";
    OS << "  Source: " << taint->sourceDescription << "\n";
    OS << "  Type: ";
    switch (taint->sourceType) {
        case TaintSourceType::USER_INPUT: OS << "USER_INPUT"; break;
        case TaintSourceType::NETWORK_INPUT: OS << "NETWORK_INPUT"; break;
        case TaintSourceType::FILE_INPUT: OS << "FILE_INPUT"; break;
        case TaintSourceType::EXTERNAL_CALL: OS << "EXTERNAL_CALL"; break;
        case TaintSourceType::CUSTOM: OS << "CUSTOM"; break;
    }
    OS << "\n";
    if (taint->sourceLocation) {
        OS << "  Location: " << getInstructionString(taint->sourceLocation) << "\n";
    }
    OS << "}\n";
}

void TaintUtils::printTaintState(const TaintState& state, raw_ostream& OS) {
    OS << "TaintState {\n";
    OS << "  Total tainted values: " << state.taintedValues.size() << "\n";
    OS << "  Value mappings: " << state.valueTaints.size() << "\n";
    
    for (const auto& pair : state.valueTaints) {
        OS << "  " << getValueName(pair.first) << " -> " << pair.second.size() << " taints\n";
    }
    OS << "}\n";
}

void TaintUtils::loadConfigFromFile(const std::string& filename,
                                  std::map<std::string, TaintSourceType>& sources,
                                  std::map<std::string, TaintSinkType>& sinks) {
    std::ifstream file(filename);
    if (!file.is_open()) return;
    
    std::string line;
    std::string section;
    
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        if (line[0] == '[' && line.back() == ']') {
            section = line.substr(1, line.length() - 2);
            continue;
        }
        
        size_t pos = line.find('=');
        if (pos == std::string::npos) continue;
        
        std::string funcName = line.substr(0, pos);
        
        if (section == "sources") {
            sources[funcName] = TaintSourceType::CUSTOM;
        } else if (section == "sinks") {
            sinks[funcName] = TaintSinkType::CUSTOM;
        }
    }
}

void TaintUtils::saveConfigToFile(const std::string& filename,
                                const std::map<std::string, TaintSourceType>& sources,
                                const std::map<std::string, TaintSinkType>& sinks) {
    std::ofstream file(filename);
    if (!file.is_open()) return;
    
    file << "# Taint Analysis Configuration\n\n";
    
    file << "[sources]\n";
    for (const auto& pair : sources) {
        file << pair.first << "=CUSTOM\n";
    }
    
    file << "\n[sinks]\n";
    for (const auto& pair : sinks) {
        file << pair.first << "=CUSTOM\n";
    }
}

// TaintStatistics implementation
void TaintStatistics::reset() {
    totalInstructions = 0;
    taintedInstructions = 0;
    sourceCalls = 0;
    sinkCalls = 0;
    sanitizerCalls = 0;
    taintFlows = 0;
    sanitizedFlows = 0;
}

void TaintStatistics::print(raw_ostream& OS) const {
    OS << "=== Taint Analysis Statistics ===\n";
    OS << "Total Instructions: " << totalInstructions << "\n";
    OS << "Tainted Instructions: " << taintedInstructions << "\n";
    OS << "Source Calls: " << sourceCalls << "\n";
    OS << "Sink Calls: " << sinkCalls << "\n";
    OS << "Sanitizer Calls: " << sanitizerCalls << "\n";
    OS << "Taint Flows: " << taintFlows << "\n";
    OS << "Sanitized Flows: " << sanitizedFlows << "\n";
    OS << "Taint Ratio: " << getTaintRatio() << "\n";
    OS << "Sanitization Ratio: " << getSanitizationRatio() << "\n";
}

double TaintStatistics::getTaintRatio() const {
    return totalInstructions > 0 ? 
           static_cast<double>(taintedInstructions) / totalInstructions : 0.0;
}

double TaintStatistics::getSanitizationRatio() const {
    return taintFlows > 0 ? 
           static_cast<double>(sanitizedFlows) / taintFlows : 0.0;
} 