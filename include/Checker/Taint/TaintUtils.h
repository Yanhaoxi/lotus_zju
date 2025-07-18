#ifndef TAINT_UTILS_H
#define TAINT_UTILS_H

#include <string>
// #include <set>
#include <map>

// Forward declarations to avoid include issues
namespace llvm {
    class Value;
    class Instruction;
    class Function;
    class raw_ostream;
}

namespace taint {

// Forward declarations
class TaintValue;
class TaintState;
enum class TaintSourceType;
enum class TaintSinkType;

// Essential utility functions for taint analysis
class TaintUtils {
public:
    // String utilities
    static std::string getValueName(const llvm::Value* val);
    static std::string getInstructionString(const llvm::Instruction* inst);
    static std::string getFunctionName(const llvm::Function* func);
    
    // Taint source/sink detection utilities
    static bool isKnownSourceFunction(const std::string& funcName);
    static bool isKnownSinkFunction(const std::string& funcName);
    static bool isKnownSanitizerFunction(const std::string& funcName);
    
    // Debug and output utilities
    static void printTaintValue(const TaintValue* taint, llvm::raw_ostream& OS);
    static void printTaintState(const TaintState& state, llvm::raw_ostream& OS);
    
    // Configuration utilities
    static void loadConfigFromFile(const std::string& filename, 
                                 std::map<std::string, TaintSourceType>& sources,
                                 std::map<std::string, TaintSinkType>& sinks);
    static void saveConfigToFile(const std::string& filename,
                               const std::map<std::string, TaintSourceType>& sources,
                               const std::map<std::string, TaintSinkType>& sinks);
};

// Taint analysis statistics
struct TaintStatistics {
    size_t totalInstructions = 0;
    size_t taintedInstructions = 0;
    size_t sourceCalls = 0;
    size_t sinkCalls = 0;
    size_t sanitizerCalls = 0;
    size_t taintFlows = 0;
    size_t sanitizedFlows = 0;
    
    void reset();
    void print(llvm::raw_ostream& OS) const;
    double getTaintRatio() const;
    double getSanitizationRatio() const;
};

} // namespace taint

#endif // TAINT_UTILS_H 