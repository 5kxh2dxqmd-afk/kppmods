foreach(abi RANGE 0 1)
    add_library(kppmods-host-${abi} SHARED host.cpp transport.cpp allocation-support.cpp)
    target_link_libraries(kppmods-host-${abi} PRIVATE kppmods-json-${abi} dl pthread)
    # Keep Boost symbols from colliding with the firmware's implementation.
    target_link_options(kppmods-host-${abi} PRIVATE "LINKER:--version-script=${CMAKE_CURRENT_SOURCE_DIR}/host.exports")
    set_property(TARGET kppmods-host-${abi} APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/host.exports")
    # Injected libraries use the firmware's shared C++ runtime and string ABI.
    set_target_properties(kppmods-host-${abi} PROPERTIES CXX_STANDARD 17 OUTPUT_NAME kppmods-host
        LIBRARY_OUTPUT_DIRECTORY "${KPPMODS_NATIVE_OUTPUT}/abi${abi}")
endforeach()
add_library(kppmods-framework-exec SHARED framework-exec.cpp allocation-support.cpp)
set_target_properties(kppmods-framework-exec PROPERTIES CXX_STANDARD 17
    LIBRARY_OUTPUT_DIRECTORY "${KPPMODS_NATIVE_OUTPUT}/lib")
target_compile_definitions(kppmods-framework-exec PRIVATE _GLIBCXX_USE_CXX11_ABI=0)
target_link_libraries(kppmods-framework-exec PRIVATE kppmods-build dl)
