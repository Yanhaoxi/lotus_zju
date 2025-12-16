#include "Dataflow/Mono/Clients/ReachableAnalysis.h"
#include "Dataflow/Mono/DataFlowEngine.h"

namespace mono {

/**
 * Reachable Analysis - A client of the monotone dataflow framework
 * 
 * This is a backward dataflow analysis that computes which instructions are
 * reachable (can be executed) forward from each program point.
 * 
 * Semantics of IN[i] and OUT[i]:
 *   - OUT[i] = Set of instructions reachable AFTER executing instruction i
 *   - IN[i]  = Set of instructions reachable FROM (starting at) instruction i
 * 
 * Dataflow equations:
 *   - GEN[i]  = {i} if filter(i) is true, otherwise empty
 *   - KILL[i] = {} (empty set, nothing is killed)
 *   - OUT[i]  = Union of IN[succ] for all successors succ of i
 *   - IN[i]   = GEN[i] ∪ OUT[i]
 * 
 * The analysis runs backward through the CFG: information flows from successors
 * to predecessors, accumulating forward reachability information.
 * 
 * Author: rainoftime
 */
std::unique_ptr<DataFlowResult> runReachableAnalysis(
    Function *f,
    const std::function<bool(Instruction *i)> &filter) {

  if (f == nullptr) {
    return nullptr;
  }

  /*
   * Allocate the engine
   */
  auto dfa = DataFlowEngine{};

  /*
   * Define the data-flow equations
   */
  auto computeGEN = [filter](Instruction *i, DataFlowResult *df) {
    /*
     * Check if the instruction should be considered.
     */
    if (!filter(i)) {
      return;
    }

    /*
     * Add the instruction to the GEN set.
     */
    auto &gen = df->GEN(i);
    gen.insert(i);

    return;
  };
  auto computeKILL = [](Instruction *, DataFlowResult *) { return; };
  auto computeOUT = [](Instruction *,
                       Instruction *succ,
                       std::set<Value *> &OUT,
                       DataFlowResult *df) {
    auto &inS = df->IN(succ);
    OUT.insert(inS.begin(), inS.end());
    return;
  };
  auto computeIN =
      [](Instruction *inst, std::set<Value *> &IN, DataFlowResult *df) {
        auto &genI = df->GEN(inst);
        auto &outI = df->OUT(inst);

        /*
         * IN[i] = GEN[i] U OUT[i]
         */
        IN.insert(genI.begin(), genI.end());
        IN.insert(outI.begin(), outI.end());

        return;
      };

  /*
   * Run the data flow analysis needed to identify the instructions that could
   * be executed from a given point.
   */
  return dfa.applyBackward(f, computeGEN, computeKILL, computeIN, computeOUT);
}

std::unique_ptr<DataFlowResult> runReachableAnalysis(Function *f) {

  /*
   * Create the function that doesn't filter out instructions.
   */
  auto noFilter = [](Instruction *) -> bool { return true; };

  /*
   * Run the analysis
   */
  return runReachableAnalysis(f, noFilter);
}

} // namespace mono

