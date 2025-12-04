/**
 * @file ModuleContext.h
 * @brief Top-level Sprattus context for an LLVM module, owning the Z3 context,
 *        configuration, and optional dynamic result store.
 */
#pragma once

#include "Analysis/Sprattus/Utils/Utils.h"
#include "Analysis/Sprattus/Utils/Config.h"
#include "Analysis/Sprattus/Core/ResultStore.h"

#include <set>
#include <z3++.h>
#include <llvm/Analysis/TargetLibraryInfo.h>

namespace llvm
{
class Function;
class CallInst;
class ReturnInst;
class DataLayout;
} // namespace llvm

namespace sprattus
{
class Analyzer;
class FunctionContext;

/**
 * Encapsulates module-wide state needed by Sprattus analyses.
 *
 * Responsible for constructing per-function `FunctionContext`s, managing
 * shared SMT infrastructure (Z3 context, data layout, library info), and
 * optionally synthesizing summary formulas for functions.
 */
class ModuleContext
{
  private:
    llvm::Module* Module_;
    configparser::Config Config_;
    mutable unique_ptr<z3::context> Z3Context_;
    unique_ptr<ResultStore> Store_;
    mutable std::set<llvm::Function*> RecurFuncs_;
    unique_ptr<llvm::DataLayout> DataLayout_;
    unique_ptr<llvm::TargetLibraryInfo> TLI_;

    z3::expr formulaForBuiltin(llvm::Function* function) const;
    z3::expr substituteReturn(z3::expr formula, ValueMapping vmap,
                              llvm::ReturnInst* ret) const;
    std::set<z3::symbol> getSharedSymbols(FunctionContext* fctx) const;

  public:
    static std::string readGlobalString(llvm::Module* module, const char* name);

    ModuleContext(llvm::Module* module, configparser::Config config);

    z3::expr formulaFor(llvm::Function* function) const;

    unique_ptr<FunctionContext> createFunctionContext(llvm::Function* f) const;

    ResultStore* getResultStore() const { return Store_.get(); }
    llvm::DataLayout* getDataLayout() const { return DataLayout_.get(); }
    llvm::TargetLibraryInfo* getTargetLibraryInfo() const { return TLI_.get(); }

    z3::context& getZ3() const { return *Z3Context_; }
    configparser::Config getConfig() const { return Config_; }

    z3::symbol getReturnSymbol() const
    {
        return Z3Context_->str_symbol("__RETURN__");
    }
};
} // namespace sprattus
