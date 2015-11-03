cmake_minimum_required(VERSION 2.6)
find_path(CMAKE_MODULE_PATH yamail.cmake
  HINTS ${CMAKE_INSTALL_PREFIX}/share/cmake_helpers
  /usr/share/cmake_helpers)
include(yamail)

yamail_project(resource_pool)

add_subdirectory(include)
add_subdirectory(tests)

yamail_configuration_message()
