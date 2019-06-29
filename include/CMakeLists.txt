find_package(Boost 1.66 REQUIRED COMPONENTS system thread REQUIRED)

add_library(resource_pool INTERFACE)
target_compile_features(resource_pool INTERFACE cxx_std_14)
target_include_directories(resource_pool
        INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>)
target_link_libraries(resource_pool INTERFACE Boost::system Boost::thread Boost::boost)
target_compile_definitions(resource_pool
        INTERFACE
        -DBOOST_COROUTINES_NO_DEPRECATION_WARNING
        )
add_library(yandex::resource_pool ALIAS resource_pool)

install(TARGETS resource_pool EXPORT resource_poolTargets
        LIBRARY DESTINATION lib COMPONENT Runtime
        ARCHIVE DESTINATION lib COMPONENT Development
        RUNTIME DESTINATION bin COMPONENT Runtime
        PUBLIC_HEADER DESTINATION include COMPONENT Development
        BUNDLE DESTINATION bin COMPONENT Runtime)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} DESTINATION .)

include(CMakePackageConfigHelpers)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/resource_pool/resource_poolConfigVersion.cmake"
        COMPATIBILITY AnyNewerVersion)

export(EXPORT resource_poolTargets
        FILE "${CMAKE_CURRENT_BINARY_DIR}/resource_pool/resource_poolTargets.cmake"
        NAMESPACE yandex::)
configure_file(../resource_poolConfig.cmake
        "${CMAKE_CURRENT_BINARY_DIR}/resource_pool/resource_poolConfig.cmake"
        COPYONLY)

set(ConfigPackageLocation lib/cmake/resource_pool)
install(EXPORT resource_poolTargets
        FILE resource_poolTargets.cmake
        NAMESPACE yandex::
        DESTINATION ${ConfigPackageLocation})
install(FILES ../resource_poolConfig.cmake
        "${CMAKE_CURRENT_BINARY_DIR}/resource_pool/resource_poolConfigVersion.cmake"
        DESTINATION ${ConfigPackageLocation}
        COMPONENT Devel)
