llvm_map_components_to_libnames(andersen_llvm_libs analysis)

add_library(Andersen SHARED
	Andersen.cpp
	AndersenAA.cpp
	ConstraintCollect.cpp
	ConstraintOptimize.cpp
	ConstraintSolving.cpp
	ExternalLibrary.cpp
	NodeFactory.cpp
)

target_compile_options(Andersen PRIVATE
	-DSPDLOG_ACTIVE_LEVEL=2
)

target_link_libraries(Andersen PRIVATE ${andersen_llvm_libs})
