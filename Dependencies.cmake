include(cmake/CPM.cmake)

# Done as a function so that updates to variables like
# CMAKE_CXX_FLAGS don't propagate out to other
# targets
function(myproject_setup_dependencies)

  # For each dependency, see if it's
  # already been provided to us by a parent project

  if(NOT TARGET Catch2::Catch2WithMain)
    cpmaddpackage("gh:catchorg/Catch2@3.12.0")
  endif()

  if(NOT TARGET CLI11::CLI11)
    cpmaddpackage("gh:CLIUtils/CLI11@2.6.1")
  endif()

  # Use vendored Chemfiles and CGAL from the repository (no per-build download
  # for these dependencies).
  if(NOT TARGET chemfiles)
    set(CHFL_BUILD_TESTS OFF CACHE BOOL "Build chemfiles unit tests" FORCE)
    set(CHFL_BUILD_DOCUMENTATION OFF CACHE BOOL "Build chemfiles documentation" FORCE)
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
