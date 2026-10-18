cmake_minimum_required(VERSION 3.25)
project(kppmods LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(KPPMODS_OUTPUT "${CMAKE_BINARY_DIR}/runtime" CACHE PATH "Runtime output directory")
set(KPPMODS_NATIVE_OUTPUT "${CMAKE_BINARY_DIR}/native" CACHE PATH "Native runtime output directory")
include(cmake/dependencies.cmake)

add_library(kppmods-build INTERFACE)
target_compile_options(kppmods-build INTERFACE -Os -Wall -Wextra -Werror -fvisibility=hidden)
target_include_directories(kppmods-build SYSTEM INTERFACE
    ${KPPMODS_BOOST_INCLUDE_DIRS} "${KPPMODS_OPENLIPC_INCLUDE_DIR}")
target_include_directories(kppmods-build INTERFACE
    "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src")
add_subdirectory(src/common)
add_subdirectory(src/host)
add_subdirectory(src/runtime)
add_custom_target(kppmods-native DEPENDS kppmods-host-0 kppmods-host-1
    kppmods-framework-exec)

include(cmake/install.cmake)
option(KPPMODS_BUILD_UI "Build bundled UI extensions" ON)
include(cmake/extensions.cmake)
