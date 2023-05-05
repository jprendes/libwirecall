cmake_minimum_required(VERSION 3.22.1)

project(wirecall VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options("-fdiagnostics-color=always")

# Dependencies
include(cmake/wirepump.cmake)
add_library(wirecall-asio ALIAS wirepump-asio)

# The main library
add_library(wirecall INTERFACE)
target_compile_features(wirecall INTERFACE cxx_std_20)
set_property(TARGET wirecall PROPERTY CXX_STANDARD 20)
target_link_libraries(wirecall INTERFACE wirepump)
target_include_directories(wirecall INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# The single-header bundle
add_executable(wirecall-bundler ALIAS wirepump-bundler)
file(GLOB_RECURSE wirecall-include-files include/*.hpp)
add_custom_command(
  OUTPUT wirecall.hpp
  DEPENDS wirecall-bundler ${wirecall-include-files} wirepump-single-header
  COMMAND wirecall-bundler
    ${CMAKE_CURRENT_SOURCE_DIR}/include/wirecall.hpp
    wirecall=${CMAKE_CURRENT_SOURCE_DIR}/include/wirecall
    wirepump=${wirepump_BINARY_DIR}/wirepump
    | sed -E 's/namespace cista/namespace wirepump::cista/'
    > wirecall.hpp
)
add_custom_target(wirecall-single-header-build DEPENDS wirecall.hpp)

add_library(wirecall-single-header INTERFACE)
target_compile_features(wirecall-single-header INTERFACE cxx_std_20)
set_property(TARGET wirecall-single-header PROPERTY CXX_STANDARD 20)
target_include_directories(wirecall-single-header INTERFACE ${CMAKE_CURRENT_BINARY_DIR})
add_dependencies(wirecall-single-header wirecall-single-header-build)

# Tests
option(wirecall_ENABLE_TESTS "Build wirecall tests" ${PROJECT_IS_TOP_LEVEL})
if(wirecall_ENABLE_TESTS)
add_subdirectory(tests)
endif()

# Install
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/wirecall.hpp TYPE INCLUDE)
