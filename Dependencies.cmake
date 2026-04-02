# Done as a function so that updates to variables like
# CMAKE_CXX_FLAGS don't propagate out to other
# targets
function(myproject_setup_dependencies)
  if(BUILD_TESTING OR WATPOCKET_BUILD_CLI)
    include(cmake/CPM.cmake)
  endif()

  # For each dependency, see if it's
  # already been provided to us by a parent project

  if(BUILD_TESTING)
    if(NOT TARGET Catch2::Catch2WithMain)
      cpmaddpackage("gh:catchorg/Catch2@3.12.0")
    endif()
  endif()

  if(WATPOCKET_BUILD_CLI)
    if(NOT TARGET CLI11::CLI11)
      cpmaddpackage("gh:CLIUtils/CLI11@2.6.1")
    endif()
  endif()

  # Use vendored Chemfiles and CGAL from the repository (no per-build download
  # for these dependencies).
  if(NOT TARGET chemfiles)
    set(CHFL_BUILD_TESTS OFF CACHE BOOL "Build chemfiles unit tests" FORCE)
    set(CHFL_BUILD_DOCUMENTATION OFF CACHE BOOL "Build chemfiles documentation" FORCE)
    # watpocket_lib is a shared library; force PIC for the vendored chemfiles
    # subtree so static chemfiles objects are linkable into shared outputs.
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    add_subdirectory(
      "${CMAKE_CURRENT_LIST_DIR}/external/chemfiles"
      "${CMAKE_CURRENT_BINARY_DIR}/external/chemfiles"
      EXCLUDE_FROM_ALL)
  endif()

  if(NOT TARGET CGAL::CGAL)
    find_package(
      CGAL
      CONFIG
      REQUIRED
      PATHS "${CMAKE_CURRENT_LIST_DIR}/external/cgal"
      NO_DEFAULT_PATH)
  endif()

endfunction()
