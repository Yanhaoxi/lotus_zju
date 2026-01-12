//
// Updated for modern LLVM compatibility
//
#include <gtest/gtest.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/SourceMgr.h>

#include "Alias/AserPTA/PointerAnalysis/Context/NoCtx.h"
#include "Alias/AserPTA/PointerAnalysis/Models/LanguageModel/DefaultLangModel/DefaultLangModel.h"
#include "Alias/AserPTA/PointerAnalysis/Models/MemoryModel/FieldSensitive/FSMemModel.h"
#include "Alias/AserPTA/PointerAnalysis/PointerAnalysisPass.h"
#include "Alias/AserPTA/PointerAnalysis/Program/CallSite.h"
#include "Alias/AserPTA/PointerAnalysis/Solver/PartialUpdateSolver.h"
#include "Alias/AserPTA/PreProcessing/Passes/CanonicalizeGEPPass.h"
#include "Alias/AserPTA/PreProcessing/Passes/InsertGlobalCtorCallPass.h"
#include "Alias/AserPTA/PreProcessing/Passes/LoweringMemCpyPass.h"
#include "Alias/AserPTA/PreProcessing/Passes/RemoveExceptionHandlerPass.h"

using namespace llvm;
using namespace aser;

#define CHECK_NO_ALIAS_FUN "__aser_no_alias__"
#define CHECK_ALIAS_FUN "__aser_alias__"

using Model = DefaultLangModel<NoCtx, FSMemModel<NoCtx>>;
using Solver = PartialUpdateSolver<Model>;
// using Solver = DeepPropagation<Model>;

cl::opt<std::string> TestIR(cl::Positional, cl::desc("path to input bitcode file"));

namespace {

class AserMarkerCallSite {
private:
    aser::CallSite CS;

    inline bool isFunNameEqualsTo(llvm::StringRef funName) const {
        if (CS.isCallOrInvoke() && llvm::isa<llvm::CallInst>(CS.getInstruction())) {
            if (const auto *fun = llvm::dyn_cast<llvm::Function>(CS.getCalledValue())) {
                return fun->getName().contains(funName);
            }
        }
        return false;
    }

public:
    explicit AserMarkerCallSite(llvm::Instruction *II) : CS(II) {}

    [[nodiscard]] inline bool isNoAliasCheck() const { return isFunNameEqualsTo(CHECK_NO_ALIAS_FUN); }

    [[nodiscard]] inline bool isAliasCheck() const { return isFunNameEqualsTo(CHECK_ALIAS_FUN); }

    [[nodiscard]] inline const llvm::Value* getArgOperand(unsigned int i) const {
        return CS.getArgOperand(i);
    }
};

template <typename Solver>
class PTAVerificationPass : public llvm::ModulePass {
public:
    using ctx = NoCtx;

    static char ID;
    PTAVerificationPass() : llvm::ModulePass(ID) {
        static_assert(std::is_same<typename Solver::ctx, NoCtx>::value, "Only support context insensitive");
    }

    void getAnalysisUsage(llvm::AnalysisUsage &AU) const override {
        AU.addRequired<PointerAnalysisPass<Solver>>();
        AU.setPreservesAll();  // does not transform the LLVM module
    }

    bool runOnModule(llvm::Module &M) override {
        this->getAnalysis<PointerAnalysisPass<Solver>>().analyze(&M, "main");

        auto &pta = *(this->getAnalysis<PointerAnalysisPass<Solver>>().getPTA());

        for (auto &F : M) {
            for (auto &BB : F) {
                for (auto &I : BB) {
                    AserMarkerCallSite CS(&I);
                    if (CS.isNoAliasCheck()) {
                        const auto *ptr1 = CS.getArgOperand(0);
                        const auto *ptr2 = CS.getArgOperand(1);

                        EXPECT_FALSE(pta.alias(nullptr, ptr1, nullptr, ptr2));

                    } else if (CS.isAliasCheck()) {
                        const auto *ptr1 = CS.getArgOperand(0);
                        const auto *ptr2 = CS.getArgOperand(1);

                        EXPECT_TRUE(pta.alias(nullptr, ptr1, nullptr, ptr2));
                    }
                }
            }
        }

        return false;
    }
};

template <typename PTA>
char PTAVerificationPass<PTA>::ID = 0;

// C++11 does not support template variables

// template <typename PTA>
// static llvm::RegisterPass<PTAVerificationPass<PTA>>
//    PVP("", "", true, true);

}  // namespace

TEST(PTACorrectness, pta_correctness) {
    SMDiagnostic Err;

    auto context = std::make_unique<LLVMContext>();
    auto module = parseIRFile(TestIR, Err, *context);

    if (!module) {
        ASSERT_FALSE(false);
        return;
    }

    llvm::legacy::PassManager passes;

    passes.add(new CanonicalizeGEPPass());
    passes.add(new LoweringMemCpyPass());
    passes.add(new RemoveExceptionHandlerPass());

    passes.add(new InsertGlobalCtorCallPass());
    passes.add(new PointerAnalysisPass<Solver>());
    passes.add(new PTAVerificationPass<Solver>());

    passes.run(*module);
}

static llvm::RegisterPass<PointerAnalysisPass<Solver>> PAP("Pointer Analysis Wrapper Pass",
                                                           "Pointer Analysis Wrapper Pass", true, true);