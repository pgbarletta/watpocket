include(GenerateExportHeader)

# Build watpocket as a shared library so consumers don't need to link internal
# dependency targets (chemfiles/CGAL) themselves.
add_library(watpocket_lib SHARED watpocket.cpp)
add_library(watpocket::watpocket ALIAS watpocket_lib)

target_link_libraries(watpocket_lib PRIVATE myproject_options myproject_warnings)

# Internal dependencies: required to build watpocket itself, but not part of the public API surface.
target_link_libraries(watpocket_lib PRIVATE chemfiles CGAL::CGAL)

target_include_directories(
  watpocket_lib
  PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
         $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
         $<INSTALL_INTERFACE:include>)

target_include_directories(watpocket_lib PRIVATE "${CMAKE_BINARY_DIR}/configured_files/include")

target_compile_features(watpocket_lib PUBLIC cxx_std_${CMAKE_CXX_STANDARD})
myproject_configure_linker(watpocket_lib)

set_target_properties(
  watpocket_lib
  PROPERTIES
    OUTPUT_NAME watpocket
    EXPORT_NAME watpocket
    VERSION ${PROJECT_VERSION}
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES)

generate_export_header(
  watpocket_lib
  EXPORT_FILE_NAME ${PROJECT_BINARY_DIR}/include/watpocket/watpocket_export.hpp)

set_target_properties(
  watpocket_lib
  PROPERTIES
    PUBLIC_HEADER
    "${PROJECT_SOURCE_DIR}/include/watpocket/watpocket.hpp;${PROJECT_BINARY_DIR}/include/watpocket/watpocket_export.hpp")
