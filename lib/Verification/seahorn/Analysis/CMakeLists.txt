add_library (SeaAnalysis STATIC
  CanAccessMemory.cc
  CanFail.cc
  CrabAnalysis.cc  
  CutPointGraph.cc
  TopologicalOrder.cc
  WeakTopologicalOrder.cc
  CanReadUndef.cc
  GateAnalysis.cc
  ControlDependenceAnalysis.cc
  ClassHierarchyAnalysis.cc
  StaticTaint.cc
  SeaBuiltinsInfo.cc
  )

# Suppress deprecation warnings
target_compile_options(SeaAnalysis PRIVATE -Wno-deprecated-declarations)

#if (HAVE_CLAM)
#  target_link_libraries (SeaAnalysis PRIVATE ${CLAM_LIBS})
#endif()
