set(SOURCES
    find_resource_pool_dependencies.cmake
    FindRESOURCE_POOL.cmake
)

set(CMAKE_INSTALL_SHAREDIR share CACHE STRING "share directory path")
install(FILES ${SOURCES} DESTINATION ${CMAKE_INSTALL_SHAREDIR}/cmake_helpers)
