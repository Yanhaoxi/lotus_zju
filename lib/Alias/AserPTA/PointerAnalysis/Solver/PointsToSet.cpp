//
// Created by peiming on 10/23/19.
//

#include <llvm/Support/CommandLine.h>
#include "Alias/AserPTA/PointerAnalysis/Solver/PointsTo/BitVectorPTS.h"
#include "Alias/AserPTA/PointerAnalysis/Solver/PointsTo/PointedByPts.h"

using namespace llvm;

llvm::cl::opt<bool> CollectStats("collect-stats", llvm::cl::desc("Dump the modified ir file"));

namespace aser {

std::vector<BitVectorPTS::PtsTy> BitVectorPTS::ptsVec;
std::vector<PointedByPts::PtsTy> PointedByPts::pointsTo;
std::vector<PointedByPts::PtsTy> PointedByPts::pointedBy;

}  // namespace aser