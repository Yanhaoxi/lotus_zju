add_library(AliasSpecManager
  AliasSpecManager.cpp
  AliasSpecManagerUtils.cpp
  AliasSpecManagerQueries.cpp
)

target_link_libraries(AliasSpecManager PUBLIC
  LotusAnnotation
)
