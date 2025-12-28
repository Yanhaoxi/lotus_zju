add_library(CanaryAliasPestrie STATIC
  # Core bitmap and data structures
  bitmap.cc
  obstack.cc
  profile_helper.cc
  
  # Tree data structures
  segtree.cc
  treap.cc
  
  # Matrix operations
  matrix-ops.cc
  
  # Pestrie core implementation
  pes-common.cc
  pes-self.cc
  pes-dual.cc
  
  # Query components
  pes-querier.cc
  bit-querier.cc
  bit-pt.cc
  bit-se.cc
)
