/**
 * @file src/llvmir-emul/llvmir_emul.cpp
 * @brief LLVM IR emulator library.
 * @copyright (c) 2017 Avast Software, licensed under the MIT license
 */

#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/GetElementPtrTypeIterator.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstVisitor.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/Format.h>
#include <llvm/Support/ManagedStatic.h>
#include <llvm/Support/MathExtras.h>
#include <llvm/Support/Memory.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/Process.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>

#include "Analysis/llvmir-emul/llvmir_emul.h"

using namespace llvm;

namespace retdec {
namespace llvmir_emul {

// Forward declarations for functions implemented in separate files
// These are defined in:
// - binary_instructions.cpp: executeFAddInst, executeFSubInst, executeFMulInst, executeFDivInst, executeFRemInst
// - comparison_instructions.cpp: executeICMP_*, executeFCMP_*, executeCmpInst
// - helper_functions.cpp: executeSelectInst, switchToNewBasicBlock, executeGEPOperation
// - conversion_instructions.cpp: executeTruncInst, executeZExtInst, executeSExtInst, etc.
// - constant_handling.cpp: getConstantExprValue, getConstantValue
// - execution_context.cpp: GlobalExecutionContext, LocalExecutionContext implementations

// Binary floating point operations
void executeFAddInst(GenericValue &Dest, GenericValue Src1, GenericValue Src2, Type *Ty);
void executeFSubInst(GenericValue &Dest, GenericValue Src1, GenericValue Src2, Type *Ty);
void executeFMulInst(GenericValue &Dest, GenericValue Src1, GenericValue Src2, Type *Ty);
void executeFDivInst(GenericValue &Dest, GenericValue Src1, GenericValue Src2, Type *Ty);
void executeFRemInst(GenericValue &Dest, GenericValue Src1, GenericValue Src2, Type *Ty);

// Comparison operations
GenericValue executeICMP_EQ(GenericValue Src1, GenericValue Src2, Type *Ty);
GenericValue executeICMP_NE(GenericValue Src1, GenericValue Src2, Type *Ty);
GenericValue executeICMP_ULT(GenericValue Src1, GenericValue Src2, Type *Ty);
GenericValue executeICMP_SLT(GenericValue Src1, GenericValue Src2, Type *Ty);
GenericValue executeICMP_UGT(GenericValue Src1, GenericValue Src2, Type *Ty);
GenericValue executeICMP_SGT(GenericValue Src1, GenericValue Src2, Type *Ty);
GenericValue executeICMP_ULE(GenericValue Src1, GenericValue Src2, Type *Ty);
GenericValue executeICMP_SLE(GenericValue Src1, GenericValue Src2, Type *Ty);
GenericValue executeICMP_UGE(GenericValue Src1, GenericValue Src2, Type *Ty);
GenericValue executeICMP_SGE(GenericValue Src1, GenericValue Src2, Type *Ty);
GenericValue executeFCMP_OEQ(GenericValue Src1, GenericValue Src2, Type *Ty);
GenericValue executeFCMP_ONE(GenericValue Src1, GenericValue Src2, Type *Ty);
GenericValue executeFCMP_OLE(GenericValue Src1, GenericValue Src2, Type *Ty);
GenericValue executeFCMP_OGE(GenericValue Src1, GenericValue Src2, Type *Ty);
GenericValue executeFCMP_OLT(GenericValue Src1, GenericValue Src2, Type *Ty);
GenericValue executeFCMP_OGT(GenericValue Src1, GenericValue Src2, Type *Ty);
GenericValue executeFCMP_UEQ(GenericValue Src1, GenericValue Src2, Type *Ty);
GenericValue executeFCMP_UNE(GenericValue Src1, GenericValue Src2, Type *Ty);
GenericValue executeFCMP_ULE(GenericValue Src1, GenericValue Src2, Type *Ty);
GenericValue executeFCMP_UGE(GenericValue Src1, GenericValue Src2, Type *Ty);
GenericValue executeFCMP_ULT(GenericValue Src1, GenericValue Src2, Type *Ty);
GenericValue executeFCMP_UGT(GenericValue Src1, GenericValue Src2, Type *Ty);
GenericValue executeFCMP_ORD(GenericValue Src1, GenericValue Src2, Type *Ty);
GenericValue executeFCMP_UNO(GenericValue Src1, GenericValue Src2, Type *Ty);
GenericValue executeFCMP_BOOL(GenericValue Src1, GenericValue Src2, Type *Ty, const bool val);

// Helper functions
GenericValue executeSelectInst(GenericValue Src1, GenericValue Src2, GenericValue Src3, Type *Ty);
void switchToNewBasicBlock(BasicBlock* Dest, LocalExecutionContext& SF, GlobalExecutionContext& GC);
GenericValue executeGEPOperation(Value *Ptr, gep_type_iterator I, gep_type_iterator E, LocalExecutionContext& SF, GlobalExecutionContext& GC);

// Conversion operations
GenericValue executeTruncInst(Value *SrcVal, Type *DstTy, LocalExecutionContext &SF, GlobalExecutionContext& GC);
GenericValue executeZExtInst(Value *SrcVal, Type *DstTy, LocalExecutionContext &SF, GlobalExecutionContext& GC);
GenericValue executeSExtInst(Value *SrcVal, Type *DstTy, LocalExecutionContext &SF, GlobalExecutionContext& GC);
GenericValue executeFPTruncInst(Value *SrcVal, Type *DstTy, LocalExecutionContext &SF, GlobalExecutionContext& GC);
GenericValue executeFPExtInst(Value *SrcVal, Type *DstTy, LocalExecutionContext &SF, GlobalExecutionContext& GC);
GenericValue executeUIToFPInst(Value *SrcVal, Type *DstTy, LocalExecutionContext &SF, GlobalExecutionContext& GC);
GenericValue executeSIToFPInst(Value *SrcVal, Type *DstTy, LocalExecutionContext &SF, GlobalExecutionContext& GC);
GenericValue executeFPToUIInst(Value *SrcVal, Type *DstTy, LocalExecutionContext &SF, GlobalExecutionContext& GC);
GenericValue executeFPToSIInst(Value *SrcVal, Type *DstTy, LocalExecutionContext &SF, GlobalExecutionContext& GC);
GenericValue executePtrToIntInst(Value *SrcVal, Type *DstTy, LocalExecutionContext &SF, GlobalExecutionContext& GC);
GenericValue executeIntToPtrInst(Value *SrcVal, Type *DstTy, LocalExecutionContext &SF, GlobalExecutionContext& GC);
GenericValue executeBitCastInst(Value *SrcVal, Type *DstTy, LocalExecutionContext &SF, GlobalExecutionContext& GC);

// Constant handling
llvm::GenericValue getConstantExprValue(llvm::ConstantExpr* CE, LocalExecutionContext& SF, GlobalExecutionContext& GC);
llvm::GenericValue getConstantValue(const llvm::Constant* C, llvm::Module* m);

namespace {

/**
 * Print any LLVM object which implements @c print(llvm::raw_string_ostream&)
 * method into std::string.
 * @param t LLVM object to print.
 * @return String with printed object.
 */
template<typename T>
std::string llvmObjToString(const T* t)
{
    std::string str;
    llvm::raw_string_ostream ss(str);
    if (t)
        t->print(ss);
    else
        ss << "nullptr";
    return ss.str();
}

} // anonymous namespace

 
 //
 //=============================================================================
 // LlvmIrEmulator
 //=============================================================================
 //
 
 LlvmIrEmulator::LlvmIrEmulator(llvm::Module* m) :
         _module(m),
         _globalEc(_module)
 {
     for (GlobalVariable& gv : _module->globals())
     {
         auto val = getConstantValue(gv.getInitializer(), _module);
         setGlobalVariableValue(&gv, val);
     }
 
     IL = new IntrinsicLowering(_module->getDataLayout());
 }
 
 LlvmIrEmulator::~LlvmIrEmulator()
 {
     delete IL;
 }
 
 llvm::GenericValue LlvmIrEmulator::runFunction(
         llvm::Function* f,
         const llvm::ArrayRef<llvm::GenericValue> argVals)
 {
     assert(_module == f->getParent());
 
     const size_t ac = f->getFunctionType()->getNumParams();
     ArrayRef<GenericValue> aargs = argVals.slice(
             0,
             std::min(argVals.size(), ac));
 
     callFunction(f, aargs);
 
     run();
 
     return _exitValue;
 }
 
 /**
  * Right now, this can not handle variadic functions. We probably will not
  * need them anyway, but if we did, it is handled in the LLVM interpreter.
  */
 void LlvmIrEmulator::callFunction(
         llvm::Function* f,
         llvm::ArrayRef<llvm::GenericValue> argVals)
 {
     _ecStack.emplace_back();
     auto& ec = _ecStack.back();
     ec.curFunction = f;
 
     if (f->isDeclaration())
     {
         assert(false && "external call unhandled");
         return;
     }
 
     ec.curBB = &f->front();
     ec.curInst = ec.curBB->begin();
 
     unsigned i = 0;
     for (auto ai = f->arg_begin(), e = f->arg_end(); ai != e; ++ai, ++i)
     {
         _globalEc.setValue(&*ai, argVals[i]);
     }
 }
 
 void LlvmIrEmulator::run()
 {
     while (!_ecStack.empty())
     {
         auto& ec = _ecStack.back();
         if (ec.curInst == ec.curBB->end())
         {
             break;
         }
         Instruction& i = *ec.curInst++;
 
         logInstruction(&i);
         visit(i);
     }
 }
 
 void LlvmIrEmulator::logInstruction(llvm::Instruction* i)
 {
     _visitedInsns.push_back(i);
     if (_visitedBbs.empty() || i->getParent() != _visitedBbs.back())
     {
         _visitedBbs.push_back(i->getParent());
     }
 }
 
 const std::list<llvm::Instruction*>& LlvmIrEmulator::getVisitedInstructions() const
 {
     return _visitedInsns;
 }
 
 const std::list<llvm::BasicBlock*>& LlvmIrEmulator::getVisitedBasicBlocks() const
 {
     return _visitedBbs;
 }
 
 bool LlvmIrEmulator::wasInstructionVisited(llvm::Instruction* i) const
 {
     for (auto* vi : getVisitedInstructions())
     {
         if (vi == i)
         {
             return true;
         }
     }
     return false;
 }
 
 bool LlvmIrEmulator::wasBasicBlockVisited(llvm::BasicBlock* bb) const
 {
     for (auto* vbb : getVisitedBasicBlocks())
     {
         if (vbb == bb)
         {
             return true;
         }
     }
     return false;
 }
 
 llvm::GenericValue LlvmIrEmulator::getExitValue() const
 {
     return _exitValue;
 }
 
 const std::list<LlvmIrEmulator::CallEntry>& LlvmIrEmulator::getCallEntries() const
 {
     return _calls;
 }
 
 std::list<llvm::Value*> LlvmIrEmulator::getCalledValues() const
 {
     std::list<llvm::Value*> ret;
     for (auto& ce : _calls)
     {
         ret.push_back(ce.calledValue);
     }
     return ret;
 }
 
 std::set<llvm::Value*> LlvmIrEmulator::getCalledValuesSet() const
 {
     std::set<llvm::Value*> ret;
     for (auto& ce : _calls)
     {
         ret.insert(ce.calledValue);
     }
     return ret;
 }
 
 /**
  * @return @c True if value @a v is called at least once.
  */
 bool LlvmIrEmulator::wasValueCalled(llvm::Value* v) const
 {
     for (auto& ce : _calls)
     {
         if (ce.calledValue == v)
         {
             return true;
         }
     }
 
     return false;
 }
 
 /**
  * @return Pointer to @c n-th call entry calling @c v value, or @c nullptr if
  *         such entry does not exist.
  */
 const LlvmIrEmulator::CallEntry* LlvmIrEmulator::getCallEntry(
         llvm::Value* v,
         unsigned n) const
 {
     unsigned cntr = 0;
     for (auto& ce : _calls)
     {
         if (ce.calledValue == v)
         {
             if (cntr == n)
             {
                 return &ce;
             }
             else
             {
                 ++cntr;
             }
         }
     }
 
     return nullptr;
 }
 
 bool LlvmIrEmulator::wasGlobalVariableLoaded(llvm::GlobalVariable* gv)
 {
     auto& c = _globalEc.globalsLoads;
     return std::find(c.begin(), c.end(), gv) != c.end();
 }
 
 bool LlvmIrEmulator::wasGlobalVariableStored(llvm::GlobalVariable* gv)
 {
     auto& c = _globalEc.globalsStores;
     return std::find(c.begin(), c.end(), gv) != c.end();
 }
 
 std::list<llvm::GlobalVariable*> LlvmIrEmulator::getLoadedGlobalVariables()
 {
     return _globalEc.globalsLoads;
 }
 
 std::set<llvm::GlobalVariable*> LlvmIrEmulator::getLoadedGlobalVariablesSet()
 {
     auto& l = _globalEc.globalsLoads;
     return std::set<GlobalVariable*>(l.begin(), l.end());
 }
 
 std::list<llvm::GlobalVariable*> LlvmIrEmulator::getStoredGlobalVariables()
 {
     return _globalEc.globalsStores;
 }
 
 std::set<llvm::GlobalVariable*> LlvmIrEmulator::getStoredGlobalVariablesSet()
 {
     auto& l = _globalEc.globalsStores;
     return std::set<GlobalVariable*>(l.begin(), l.end());
 }
 
 llvm::GenericValue LlvmIrEmulator::getGlobalVariableValue(
         llvm::GlobalVariable* gv)
 {
     return _globalEc.getGlobal(gv, false);
 }
 
 void LlvmIrEmulator::setGlobalVariableValue(
         llvm::GlobalVariable* gv,
         llvm::GenericValue val)
 {
     _globalEc.setGlobal(gv, val, false);
 }
 
 bool LlvmIrEmulator::wasMemoryLoaded(uint64_t addr)
 {
     auto& c = _globalEc.memoryLoads;
     return std::find(c.begin(), c.end(), addr) != c.end();
 }
 
 bool LlvmIrEmulator::wasMemoryStored(uint64_t addr)
 {
     auto& c = _globalEc.memoryStores;
     return std::find(c.begin(), c.end(), addr) != c.end();
 }
 
 std::list<uint64_t> LlvmIrEmulator::getLoadedMemory()
 {
     return _globalEc.memoryLoads;
 }
 
 std::set<uint64_t> LlvmIrEmulator::getLoadedMemorySet()
 {
     auto& l = _globalEc.memoryLoads;
     return std::set<uint64_t>(l.begin(), l.end());
 }
 
 std::list<uint64_t> LlvmIrEmulator::getStoredMemory()
 {
     return _globalEc.memoryStores;
 }
 
 std::set<uint64_t> LlvmIrEmulator::getStoredMemorySet()
 {
     auto& l = _globalEc.memoryStores;
     return std::set<uint64_t>(l.begin(), l.end());
 }
 
 llvm::GenericValue LlvmIrEmulator::getMemoryValue(uint64_t addr)
 {
     return _globalEc.getMemory(addr, false);
 }
 
 void LlvmIrEmulator::setMemoryValue(uint64_t addr, llvm::GenericValue val)
 {
     _globalEc.setMemory(addr, val, false);
 }
 
 /**
  * Get generic value for the passed LLVM value @a val.
  * If @c val is a global variable, result of @c getGlobalVariableValue() is
  * returned.
  * Otherwise, LLVM value to generic value map in global context is used.
  */
 llvm::GenericValue LlvmIrEmulator::getValueValue(llvm::Value* val)
 {
     if (auto* gv = dyn_cast<GlobalVariable>(val))
     {
         return getGlobalVariableValue(gv);
     }
     else
     {
         return _globalEc.values[val];
     }
 }
 
//
//=============================================================================
// Terminator Instruction Implementations
//=============================================================================
//

void LlvmIrEmulator::popStackAndReturnValueToCaller(
        llvm::Type* retT,
        llvm::GenericValue res)
{
    _ecStackRetired.emplace_back(_ecStack.back());
    _ecStack.pop_back();

    // Finished main. Put result into exit code...
    //
    if (_ecStack.empty())
    {
        if (retT && !retT->isVoidTy())
        {
            _exitValue = res;
        }
        else
        {
            // Matula: This memset is ok.
            memset(&_exitValue.Untyped, 0, sizeof(_exitValue.Untyped));
        }
    }
    // If we have a previous stack frame, and we have a previous call,
    // fill in the return value...
    //
    else
    {
       LocalExecutionContext& callingEc = _ecStack.back();
        if (CallBase* CB = callingEc.caller)
        {
            // Save result...
            if (!CB->getType()->isVoidTy())
            {
                _globalEc.setValue(CB, res);
            }
            if (InvokeInst* II = dyn_cast<InvokeInst>(CB))
            {
                switchToNewBasicBlock(II->getNormalDest (), callingEc, _globalEc);
            }
            // We returned from the call...
            callingEc.caller = nullptr;
        }
    }
}

// Terminator instruction visitors are implemented in instruction_visitors_terminator.cpp

//
//=============================================================================
// Super Instruction Implementations
//=============================================================================
//

/**
 * When visitor does not find visit method for a particular child class,
 * it uses visit method for the parent class. This is a visit for the super
 * parent class for all LLVM instructions. If visitor gets here, it means
 * the current instruction is not handled -- it should have its own specialized
 * visit method, no instruction should be handled by this super visit method.
 */
void LlvmIrEmulator::visitInstruction(llvm::Instruction& I)
{
    throw LlvmIrEmulatorError(
            "Unhandled instruction visited: " + llvmObjToString(&I));
}

} // llvmir_emul
} // retdec
