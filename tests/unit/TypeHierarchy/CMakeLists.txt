# CMakeLists.txt for TypeHierarchy unit tests

# Add DIBasedTypeHierarchy tests
add_lotus_test(dibased_type_hierarchy_test
  DIBasedTypeHierarchyTest.cpp
)

# Add DIBasedTypeHierarchy serialization tests
add_lotus_test(dibased_type_hierarchy_serialization_test
  DIBasedTypeHierarchySerializationTest.cpp
)