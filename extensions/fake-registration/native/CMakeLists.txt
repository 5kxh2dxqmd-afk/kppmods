add_library(kppmods-fake-registration SHARED registration.cpp)
set_target_properties(kppmods-fake-registration PROPERTIES CXX_STANDARD 17 LIBRARY_OUTPUT_DIRECTORY "${KPPMODS_NATIVE_OUTPUT}/modules")
target_link_libraries(kppmods-fake-registration PRIVATE kppmods-build dl)
