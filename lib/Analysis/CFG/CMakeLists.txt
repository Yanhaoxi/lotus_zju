set(CanaryCFG_SOURCES
  CFGReachability.cpp
  Dominator.cpp
  DominatorForest.cpp
  DominatorNode.cpp
  # SotoTopo.cpp  # require boost, to decide whther keep it
  # TopologicalOrder.cpp
  # WeakTopologicalOrder.cpp
)
add_library(CanaryCFG STATIC ${CanaryCFG_SOURCES})
target_link_libraries(CanaryCFG CanaryLLVMUtils)

