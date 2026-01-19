#ifndef ANALYSIS_LLVMANALYSISDOMAIN_H_
#define ANALYSIS_LLVMANALYSISDOMAIN_H_

#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Value.h"

namespace mono {

template <typename ContainerT> struct LLVMMonoAnalysisDomain {
  using n_t = llvm::Instruction *;
  using d_t = llvm::Value *;
  using f_t = llvm::Function *;
  using mono_container_t = ContainerT;
};

} // namespace mono

#endif // ANALYSIS_LLVMANALYSISDOMAIN_H_
