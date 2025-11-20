#ifndef NPA_INTERPROC_RD_H
#define NPA_INTERPROC_RD_H

#include "Dataflow/NPA/Domains/GenKillDomain.h"
#include <llvm/IR/Module.h>
#include <map>

namespace npa {

class InterproceduralRD {
public:
    struct Result {
        // Phase 1: Function Summaries (Kill, Gen)
        std::map<const llvm::Function*, GenKillDomain::value_type> summaries;
        
        // Phase 2: Context-Insensitive Facts at Block Entry
        std::map<const llvm::BasicBlock*, llvm::APInt> blockFacts; 
    };

    static Result run(llvm::Module &M, bool verbose = false);
};

} // namespace npa
#endif
