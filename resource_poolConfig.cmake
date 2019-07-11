include(CMakeFindDependencyMacro)
find_dependency(Boost 1.66 COMPONENTS system thread)

include("${CMAKE_CURRENT_LIST_DIR}/resource_poolTargets.cmake")
