/*
 * Taint Configuration Manager
 * 
 * Simple singleton for managing taint configurations
 */

#pragma once

#include "Annotation/Taint/TaintConfigParser.h"
#include <llvm/IR/Instructions.h>
#include <memory>
#include <string>


// Simple singleton manager
class TaintConfigManager {
private:
    static std::unique_ptr<TaintConfigManager> instance;
    std::unique_ptr<TaintConfig> config;
    
    // Function name normalization: handles platform-specific prefixes and variants
    static std::string normalize_function_name(const std::string& func_name) {
        std::string normalized = func_name;
        
        // Strip platform-specific prefixes (e.g., "\01_" on macOS/Darwin)
        if (normalized.length() > 2 && normalized[0] == 1 && normalized[1] == '_') {
            normalized = normalized.substr(2);
        }
        
        // Handle fortified versions (e.g., "__strcpy_chk" → "strcpy")
        // These are security-hardened versions used on macOS and some Linux systems
        if (normalized.find("__") == 0 && normalized.find("_chk") == normalized.length() - 4) {
            normalized = normalized.substr(2, normalized.length() - 6);
        }
        
        return normalized;
    }
    
public:
    TaintConfigManager() {}
    
    static TaintConfigManager& getInstance() {
        if (!instance) {
            instance = std::make_unique<TaintConfigManager>();
        }
        return *instance;
    }
    
    bool load_config(const std::string& config_file) {
        config = TaintConfigParser::parse_file(config_file);
        return config != nullptr;
    }
    
    bool load_config_quiet(const std::string& config_file) {
        config = TaintConfigParser::parse_file_quiet(config_file);
        return config != nullptr;
    }
    
    bool load_default_config() {
        // Try multiple possible paths for the config file
        std::vector<std::string> possible_paths = {
            "config/taint.spec",           // From project root
            "../config/taint.spec",        // From build directory
            "../../config/taint.spec",     // From build/bin directory
            "../../../config/taint.spec"   // From build/tests directory
        };
        
        for (const auto& path : possible_paths) {
            if (load_config_quiet(path)) {
                return true;
            }
        }
        
        llvm::errs() << "Error: Could not find taint config file in any of the expected locations\n";
        return false;
    }
    
    bool is_source(const std::string& func_name) const {
        if (!config) return false;
        std::string normalized = normalize_function_name(func_name);
        return config->is_source(normalized);
    }
    
    bool is_sink(const std::string& func_name) const {
        if (!config) return false;
        std::string normalized = normalize_function_name(func_name);
        return config->is_sink(normalized);
    }
    
    bool is_ignored(const std::string& func_name) const {
        if (!config) return false;
        std::string normalized = normalize_function_name(func_name);
        return config->is_ignored(normalized);
    }
    
    bool is_source(const llvm::CallInst* call) const {
        if (!call) return false;
        const llvm::Function* callee = call->getCalledFunction();
        return callee && is_source(callee->getName().str());
    }
    
    bool is_sink(const llvm::CallInst* call) const {
        if (!call) return false;
        const llvm::Function* callee = call->getCalledFunction();
        return callee && is_sink(callee->getName().str());
    }
    
    void dump_config(llvm::raw_ostream& OS) const {
        if (config) config->dump(OS);
    }
    
    size_t get_source_count() const {
        return config ? config->sources.size() : 0;
    }
    
    size_t get_sink_count() const {
        return config ? config->sinks.size() : 0;
    }
    
    std::vector<std::string> get_all_source_functions() const {
        std::vector<std::string> result;
        if (config) {
            for (const auto& func : config->sources) {
                result.push_back(func);
            }
        }
        return result;
    }
    
    std::vector<std::string> get_all_sink_functions() const {
        std::vector<std::string> result;
        if (config) {
            for (const auto& func : config->sinks) {
                result.push_back(func);
            }
        }
        return result;
    }
    
    const FunctionTaintConfig* get_function_config(const std::string& func_name) const {
        if (!config) return nullptr;
        std::string normalized = normalize_function_name(func_name);
        return config->get_function_config(normalized);
    }
    
    // Expose normalization for external use
    static std::string get_normalized_name(const std::string& func_name) {
        return normalize_function_name(func_name);
    }
};

// Convenience namespace
namespace taint_config {
    inline bool is_source(const std::string& func_name) {
        return TaintConfigManager::getInstance().is_source(func_name);
    }
    
    inline bool is_sink(const std::string& func_name) {
        return TaintConfigManager::getInstance().is_sink(func_name);
    }
    
    inline bool is_ignored(const std::string& func_name) {
        return TaintConfigManager::getInstance().is_ignored(func_name);
    }
    
    inline bool is_source(const llvm::CallInst* call) {
        return TaintConfigManager::getInstance().is_source(call);
    }
    
    inline bool is_sink(const llvm::CallInst* call) {
        return TaintConfigManager::getInstance().is_sink(call);
    }
    
    inline bool load_config(const std::string& config_file) {
        return TaintConfigManager::getInstance().load_config(config_file);
    }
    
    inline bool load_default_config() {
        return TaintConfigManager::getInstance().load_default_config();
    }
    
    inline void dump_config(llvm::raw_ostream& OS) {
        TaintConfigManager::getInstance().dump_config(OS);
    }
    
    inline size_t get_source_count() {
        return TaintConfigManager::getInstance().get_source_count();
    }
    
    inline size_t get_sink_count() {
        return TaintConfigManager::getInstance().get_sink_count();
    }
    
    inline const FunctionTaintConfig* get_function_config(const std::string& func_name) {
        return TaintConfigManager::getInstance().get_function_config(func_name);
    }
    
    inline std::string normalize_name(const std::string& func_name) {
        return TaintConfigManager::get_normalized_name(func_name);
    }
} // namespace taint_config
