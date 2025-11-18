/**
 * @file src/llvmir-emul/instruction_visitors_misc.cpp
 * @brief Miscellaneous instruction visitor implementations.
 * @copyright (c) 2017 Avast Software, licensed under the MIT license
 */

#include "Analysis/llvmir-emul/llvmir_emul.h"
#include <llvm/IR/Instructions.h>

using namespace llvm;

namespace retdec {
namespace llvmir_emul {
 void LlvmIrEmulator::visitVAArgInst(llvm::VAArgInst& I)
 {
     assert(false && "Handling of VAArgInst is not implemented");
     throw LlvmIrEmulatorError("Handling of VAArgInst is not implemented");
 }
 
 /**
  * This is not really getting the value. It just sets ExtractValueInst's result
  * to uninitialized GenericValue.
  */
 void LlvmIrEmulator::visitExtractElementInst(llvm::ExtractElementInst& I)
 {
     GenericValue dest;
     _globalEc.setValue(&I, dest);
 }
 
 void LlvmIrEmulator::visitInsertElementInst(llvm::InsertElementInst& I)
 {
     assert(false && "Handling of InsertElementInst is not implemented");
     throw LlvmIrEmulatorError("Handling of InsertElementInst is not implemented");
 }
 
 void LlvmIrEmulator::visitShuffleVectorInst(llvm::ShuffleVectorInst& I)
 {
     assert(false && "Handling of ShuffleVectorInst is not implemented");
     throw LlvmIrEmulatorError("Handling of ShuffleVectorInst is not implemented");
 }
 
 /**
  * This is not really getting the value. It just sets ExtractValueInst's result
  * to uninitialized GenericValue.
  */
 void LlvmIrEmulator::visitExtractValueInst(llvm::ExtractValueInst& I)
 {
     GenericValue dest;
     _globalEc.setValue(&I, dest);
 }
 
 void LlvmIrEmulator::visitInsertValueInst(llvm::InsertValueInst& I)
 {
     assert(false && "Handling of InsertValueInst is not implemented");
     throw LlvmIrEmulatorError("Handling of InsertValueInst is not implemented");
 }
 
 void LlvmIrEmulator::visitPHINode(llvm::PHINode& PN)
 {
     throw LlvmIrEmulatorError("PHI nodes already handled!");
 }

} // llvmir_emul
} // retdec
