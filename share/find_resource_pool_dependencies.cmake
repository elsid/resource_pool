function(find_resource_pool_dependencies)

find_package(Boost REQUIRED system thread)

set(RESOURCE_POOL_DEPENDENCY_INCLUDE_DIRS
  ${Boost_INCLUDE_DIR}
  CACHE INTERNAL ""
)

set(RESOURCE_POOL_DEPENDENCY_LIBRARIES
  ${Boost_LIBRARIES}
  CACHE INTERNAL ""
)

endfunction()
