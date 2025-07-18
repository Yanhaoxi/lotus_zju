// This is a naive implementation of taint analysis.
#include "Checker/Taint/TaintAnalysis.h"
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/raw_ostream.h>
// #include <algorithm>
// #include <iostream>

using namespace llvm;
using namespace taint;

// TaintState implementation
void TaintState::addTaint(llvm::Value* val, TaintValue* taint) {
    taintedValues.insert(taint);
    valueTaints[val].insert(taint);
}

void TaintState::removeTaint(llvm::Value* val) {
    auto it = valueTaints.find(val);
    if (it != valueTaints.end()) {
        for (auto* taint : it->second) {
            taintedValues.erase(taint);
        }
        valueTaints.erase(it);
    }
}

bool TaintState::isTainted(llvm::Value* val) const {
    auto it = valueTaints.find(val);
    return it != valueTaints.end() && !it->second.empty();
}

std::set<TaintValue*> TaintState::getTaints(llvm::Value* val) const {
    auto it = valueTaints.find(val);
    return it != valueTaints.end() ? it->second : std::set<TaintValue*>();
}

void TaintState::merge(const TaintState& other) {
    for (const auto& pair : other.valueTaints) {
        for (auto* taint : pair.second) {
            addTaint(pair.first, taint);
        }
    }
}

void TaintState::clear() {
    taintedValues.clear();
    valueTaints.clear();
}

bool TaintState::operator==(const TaintState& other) const {
    return valueTaints == other.valueTaints;
}

// TaintConfig implementation
TaintConfig::TaintConfig() {
    loadDefaultConfig();
}

void TaintConfig::loadDefaultConfig() {
    // Sources
    sourceFunctions = {
        {"gets", TaintSourceType::USER_INPUT},
        {"fgets", TaintSourceType::USER_INPUT},
        {"scanf", TaintSourceType::USER_INPUT},
        {"read", TaintSourceType::FILE_INPUT},
        {"recv", TaintSourceType::NETWORK_INPUT},
        {"getenv", TaintSourceType::USER_INPUT}
    };
    
    // Sinks
    sinkFunctions = {
        {"system", TaintSinkType::SYSTEM_CALL},
        {"exec", TaintSinkType::SYSTEM_CALL},
        {"strcpy", TaintSinkType::MEMORY_WRITE},
        {"printf", TaintSinkType::FILE_WRITE}
    };
    
    // Sanitizers
    sanitizerFunctions = {
        {"strlen", SanitizerType::BOUNDS_CHECK},
        {"strncpy", SanitizerType::BOUNDS_CHECK}
    };
}

// TaintAnalysisResult implementation
void TaintAnalysisResult::addFlow(const TaintFlow& flow) {
    flows.push_back(flow);
}

void TaintAnalysisResult::addTaint(TaintValue* taint) {
    allTaints.insert(taint);
}

void TaintAnalysisResult::setState(llvm::Function* func, llvm::Instruction* inst, const TaintState& state) {
    functionStates[func][inst] = state;
}

TaintState TaintAnalysisResult::getState(llvm::Function* func, llvm::Instruction* inst) const {
    auto funcIt = functionStates.find(func);
    if (funcIt != functionStates.end()) {
        auto instIt = funcIt->second.find(inst);
        if (instIt != funcIt->second.end()) {
            return instIt->second;
        }
    }
    return TaintState();
}

void TaintAnalysisResult::printResults(llvm::raw_ostream& OS) const {
    OS << "=== Taint Analysis Results ===\n";
    OS << "Total Taints: " << allTaints.size() << "\n";
    OS << "Total Flows: " << flows.size() << "\n\n";
    
    for (size_t i = 0; i < flows.size(); ++i) {
        const auto& flow = flows[i];
        OS << "Flow " << (i + 1) << ": " << flow.source->sourceDescription;
        OS << " -> Sink (sanitized: " << (flow.sanitized ? "Yes" : "No") << ")\n";
    }
}

void TaintAnalysisResult::printFlows(llvm::raw_ostream& OS) const {
    printResults(OS);
}

void TaintAnalysisResult::printStatistics(llvm::raw_ostream& OS) const {
    printResults(OS);
}

// TaintAnalysis implementation
void TaintAnalysis::analyzeModule(llvm::Module* M) {
    for (auto& F : *M) {
        if (!F.isDeclaration() && !F.empty()) {
            analyzeFunction(&F);
        }
    }
}

bool TaintAnalysis::isSourceFunction(const llvm::Function* func) const {
    return func && config.sourceFunctions.count(func->getName().str());
}

bool TaintAnalysis::isSinkFunction(const llvm::Function* func) const {
    return func && config.sinkFunctions.count(func->getName().str());
}

bool TaintAnalysis::isSanitizerFunction(const llvm::Function* func) const {
    return func && config.sanitizerFunctions.count(func->getName().str());
}

TaintSourceType TaintAnalysis::getSourceType(const llvm::Function* func) const {
    if (!func) return TaintSourceType::CUSTOM;
    auto it = config.sourceFunctions.find(func->getName().str());
    return it != config.sourceFunctions.end() ? it->second : TaintSourceType::CUSTOM;
}

TaintSinkType TaintAnalysis::getSinkType(const llvm::Function* func) const {
    if (!func) return TaintSinkType::CUSTOM;
    auto it = config.sinkFunctions.find(func->getName().str());
    return it != config.sinkFunctions.end() ? it->second : TaintSinkType::CUSTOM;
}

SanitizerType TaintAnalysis::getSanitizerType(const llvm::Function* func) const {
    if (!func) return SanitizerType::CUSTOM;
    auto it = config.sanitizerFunctions.find(func->getName().str());
    return it != config.sanitizerFunctions.end() ? it->second : SanitizerType::CUSTOM;
}

void TaintAnalysis::analyzeFunction(llvm::Function* func) {
    TaintState state;
    
    // Taint main function arguments
    if (func->getName() == "main") {
        for (auto& arg : func->args()) {
            auto* taint = createTaintValue(&arg, TaintSourceType::USER_INPUT, 
                                         &*func->begin()->begin(), "Command line argument");
            state.addTaint(&arg, taint);
            result.addTaint(taint);
        }
    }
    
    // Analyze instructions
    for (auto& BB : *func) {
        for (auto& I : BB) {
            analyzeInstruction(&I, state);
            result.setState(func, &I, state);
        }
    }
}

void TaintAnalysis::analyzeInstruction(llvm::Instruction* inst, TaintState& state) {
    if (auto* call = dyn_cast<CallInst>(inst)) {
        analyzeCallInstruction(call, state);
    } else if (auto* load = dyn_cast<LoadInst>(inst)) {
        if (config.trackThroughMemory && state.isTainted(load->getPointerOperand())) {
            propagateTaint(load->getPointerOperand(), load, state);
        }
    } else if (auto* store = dyn_cast<StoreInst>(inst)) {
        if (config.trackThroughMemory && state.isTainted(store->getValueOperand())) {
            propagateTaint(store->getValueOperand(), store->getPointerOperand(), state);
        }
    } else if (auto* binOp = dyn_cast<BinaryOperator>(inst)) {
        if (state.isTainted(binOp->getOperand(0)) || state.isTainted(binOp->getOperand(1))) {
            propagateTaint(state.isTainted(binOp->getOperand(0)) ? binOp->getOperand(0) : binOp->getOperand(1), binOp, state);
        }
    }
}

void TaintAnalysis::analyzeCallInstruction(llvm::CallInst* call, TaintState& state) {
    llvm::Function* func = call->getCalledFunction();
    if (!func) return;
    
    std::string name = func->getName().str();
    
    // Handle source functions
    if (isSourceFunction(func)) {
        auto* taint = createTaintValue(call, getSourceType(func), call, "Call to " + name);
        if (name == "gets" || name == "fgets" || name == "scanf") {
            if (call->arg_size() > 0) {
                state.addTaint(call->getArgOperand(0), taint);
            }
        }
        state.addTaint(call, taint);
        result.addTaint(taint);
    }
    
    // Handle sink functions
    if (isSinkFunction(func)) {
        for (unsigned i = 0; i < call->arg_size(); ++i) {
            if (state.isTainted(call->getArgOperand(i))) {
                auto taints = state.getTaints(call->getArgOperand(i));
                for (auto* taint : taints) {
                    result.addFlow(TaintFlow(taint, call, getSinkType(func)));
                }
            }
        }
    }
    
    // Handle sanitizers
    if (isSanitizerFunction(func) && call->arg_size() > 0) {
        state.removeTaint(call);
    }
    
    // Propagate taint through calls
    if (config.trackThroughCalls && !call->getType()->isVoidTy()) {
        for (unsigned i = 0; i < call->arg_size(); ++i) {
            if (state.isTainted(call->getArgOperand(i))) {
                auto* taint = createTaintValue(call, TaintSourceType::EXTERNAL_CALL, call, "Propagated through " + name);
                state.addTaint(call, taint);
                result.addTaint(taint);
                break;
            }
        }
    }
}

void TaintAnalysis::propagateTaint(llvm::Value* from, llvm::Value* to, TaintState& state) {
    auto taints = state.getTaints(from);
    for (auto* taint : taints) {
        auto* newTaint = createTaintValue(to, taint->sourceType, taint->sourceLocation, 
                                        taint->sourceDescription + " (propagated)");
        newTaint->derivedFrom.insert(taint);
        state.addTaint(to, newTaint);
        result.addTaint(newTaint);
    }
}

void TaintAnalysis::checkForTaintFlow(llvm::Instruction* inst, const TaintState& state) {
    // Simplified - main logic is in analyzeCallInstruction
}

TaintValue* TaintAnalysis::createTaintValue(llvm::Value* val, TaintSourceType type, 
                                          llvm::Instruction* loc, const std::string& desc) {
    return new TaintValue(val, type, loc, desc);
}
