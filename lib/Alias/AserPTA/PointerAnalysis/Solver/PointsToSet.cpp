//
// Created by peiming on 10/23/19.
//

#include "Alias/AserPTA/PointerAnalysis/Solver/PointsTo/BitVectorPTS.h"
#include "Alias/AserPTA/PointerAnalysis/Solver/PointsTo/BDDPts.h"
#include "Alias/AserPTA/PointerAnalysis/Solver/PointsTo/PointedByPts.h"
#include <llvm/Support/CommandLine.h>

using namespace llvm;

llvm::cl::opt<bool> CollectStats("collect-stats", llvm::cl::desc("Dump the modified ir file"));

namespace aser {

// Command line option definition (declared as extern in BDDPts.h)
llvm::cl::opt<bool> ConfigUseBDDPts(
    "pta-use-bdd-pts",
    llvm::cl::desc("Use BDD-backed points-to sets instead of SparseBitVector"),
    llvm::cl::init(false));

std::vector<BitVectorPTS::PtsTy> BitVectorPTS::ptsVec;
std::vector<ConfigurablePTS::PtsTy> ConfigurablePTS::ptsVec;
bool ConfigurablePTS::useBDDBackend = false;
bool ConfigurablePTS::backendLocked = false;
std::vector<PointedByPts::PtsTy> PointedByPts::pointsTo;
std::vector<PointedByPts::PtsTy> PointedByPts::pointedBy;

}  // namespace aser