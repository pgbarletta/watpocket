cmake_minimum_required(VERSION 3.28)

# Disable modules support, this is broken with clang-tidy at the moment
cmake_policy(SET CMP0155 OLD)

set(WATPOCKET_DEFAULT_VERSION "0.0.2")

if(NOT DEFINED WATPOCKET_VERSION OR WATPOCKET_VERSION STREQUAL "")
  set(WATPOCKET_VERSION "${WATPOCKET_DEFAULT_VERSION}")
endif()

# This template attempts to be "fetch_content"-able
# so that it works well with tools like CPM or other
# manual dependency management

# Only set the cxx_standard if it is not set by someone else
if (NOT DEFINED CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 23)
endif()

# strongly encouraged to enable this globally to avoid conflicts between
# -Wpedantic being enabled and -std=c++20 and -std=gnu++20 for example
# when compiling with PCH enabled
set(CMAKE_CXX_EXTENSIONS OFF)

# Set the project name and language
project(
  myproject
  VERSION ${WATPOCKET_VERSION}
  DESCRIPTION ""
  HOMEPAGE_URL "%%myurl%%"
  LANGUAGES CXX C)

include(cmake/PreventInSourceBuilds.cmake)
include(cmake/Emscripten.cmake)
include(ProjectOptions.cmake)

option(WATPOCKET_ENABLE_PYTHON_BINDINGS "Build Python bindings via nanobind" OFF)
option(WATPOCKET_BUILD_CLI "Build the watpocket CLI executable" ON)
if(SKBUILD)
  set(WATPOCKET_ENABLE_PYTHON_BINDINGS ON CACHE BOOL "Build Python bindings via nanobind" FORCE)
  set(WATPOCKET_BUILD_CLI OFF CACHE BOOL "Build the watpocket CLI executable" FORCE)
  set(BUILD_TESTING OFF CACHE BOOL "Enable tests for this project" FORCE)
endif()

myproject_setup_options()

myproject_global_options()
include(CTest)
include(Dependencies.cmake)
myproject_setup_dependencies()

myproject_local_options()

set(CMAKE_CXX_VISIBILITY_PRESET hidden)

if(NOT DEFINED GIT_SHA OR GIT_SHA STREQUAL "")
  set(GIT_SHA "Unknown")
endif()

set(GIT_SHA
    "${GIT_SHA}"
    CACHE STRING "SHA this build was generated from"
    FORCE)
string(
  SUBSTRING "${GIT_SHA}"
            0
            8
            GIT_SHORT_SHA)

target_compile_features(myproject_options INTERFACE cxx_std_${CMAKE_CXX_STANDARD})

add_library(myproject::myproject_options ALIAS myproject_options)
add_library(myproject::myproject_warnings ALIAS myproject_warnings)

#add_library(myproject::myproject_options INTERFACE IMPORTED)
#add_library(myproject::myproject_warnings INTERFACE IMPORTED)

# configure files based on CMake configuration options
add_subdirectory(configured_files)

# Adding the src:
add_subdirectory(src)

# Create unified web deployment directory (for WASM builds)
if(EMSCRIPTEN)
  myproject_create_web_dist()
endif()

# Don't even look at tests if we're not top level
if(NOT PROJECT_IS_TOP_LEVEL)
  return()
endif()

# Adding the tests:

if(BUILD_TESTING)
  message(AUTHOR_WARNING "Building tests (see test/ for CLI smoke tests and API sanity checks)")
  add_subdirectory(test)
endif()


# If MSVC is being used, and ASAN is enabled, we need to set the debugger environment
# so that it behaves well with MSVC's debugger, and we can run the target from visual studio
if(MSVC)
  get_all_installable_targets(all_targets)
  message("all_targets=${all_targets}")
  set_target_properties(${all_targets} PROPERTIES VS_DEBUGGER_ENVIRONMENT "PATH=$(VC_ExecutablePath_x64);%PATH%")
endif()

# set the startup project for the "play" button in MSVC
if(TARGET watpocket)
  set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT watpocket)
endif()

if(CMAKE_SKIP_INSTALL_RULES)
  return()
endif()

if(SKBUILD)
  return()
endif()

include(cmake/PackageProject.cmake)

# Add other targets that you want installed here, by default we just package the one executable
# we know we want to ship
myproject_package_project(
  NAME
  watpocket
  TARGETS
  watpocket_lib
  # FIXME: this does not work! CK
  # PRIVATE_DEPENDENCIES_CONFIGURED project_options project_warnings
)

# Install the CLI binary as a runtime artifact, but do not export it as part of
# the CMake package to avoid name conflicts with the `watpocket` library target.
include(GNUInstallDirs)
if(TARGET watpocket)
  install(TARGETS watpocket RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT bin)
endif()

# Experience shows that explicit package naming can help make it easier to sort
# out potential ABI related issues before they start, while helping you
# track a build to a specific GIT SHA
set(CPACK_PACKAGE_FILE_NAME
    "${CMAKE_PROJECT_NAME}-${CMAKE_PROJECT_VERSION}-${GIT_SHORT_SHA}-${CMAKE_SYSTEM_NAME}-${CMAKE_BUILD_TYPE}-${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
)

include(CPack)
