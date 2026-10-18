set(KPPMODS_BOOST_ROOT "${PROJECT_SOURCE_DIR}/vendor/boost")
foreach(component IN ITEMS json describe asio process)
    if(NOT EXISTS "${KPPMODS_BOOST_ROOT}/libs/${component}/include")
        message(FATAL_ERROR "Initialize upstreams with tools/build/init-dependencies.sh")
    endif()
endforeach()
file(GLOB KPPMODS_BOOST_INCLUDE_DIRS CONFIGURE_DEPENDS
    "${KPPMODS_BOOST_ROOT}/libs/*/include"
    "${KPPMODS_BOOST_ROOT}/libs/numeric/*/include")

# Apply the Kindle hash-array declarations to a build copy of the upstream header.
find_program(KPPMODS_PATCH_EXECUTABLE patch REQUIRED)
set(KPPMODS_OPENLIPC_INCLUDE_DIR "${CMAKE_BINARY_DIR}/dependencies/openlipc")
set(KPPMODS_OPENLIPC_STAGING_DIR "${KPPMODS_OPENLIPC_INCLUDE_DIR}/staging")
file(MAKE_DIRECTORY "${KPPMODS_OPENLIPC_STAGING_DIR}")
configure_file("${PROJECT_SOURCE_DIR}/vendor/openlipc/include/openlipc.h"
    "${KPPMODS_OPENLIPC_STAGING_DIR}/openlipc.h" COPYONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/patches/openlipc.patch")
execute_process(COMMAND "${KPPMODS_PATCH_EXECUTABLE}" -s -p1 -i
    "${CMAKE_CURRENT_LIST_DIR}/patches/openlipc.patch"
    WORKING_DIRECTORY "${KPPMODS_OPENLIPC_STAGING_DIR}"
    COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${KPPMODS_OPENLIPC_STAGING_DIR}/openlipc.h"
    "${KPPMODS_OPENLIPC_INCLUDE_DIR}/openlipc.h"
    COMMAND_ERROR_IS_FATAL ANY)
