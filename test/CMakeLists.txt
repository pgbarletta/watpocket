cmake_minimum_required(VERSION 3.15...3.23)

project(CmakeConfigPackageTests 
  VERSION ${PROJECT_VERSION}
  LANGUAGES CXX)

# ---- Test as standalone project the exported config package ----

if(PROJECT_IS_TOP_LEVEL OR TEST_INSTALLED_VERSION)
  enable_testing()

  find_package(myproject CONFIG REQUIRED) # for watpocket, project_options, ...

  if(NOT TARGET myproject_options)
    message(FATAL_ERROR "Required config package not found!")
    return() # be strictly paranoid for Template Janitor github action! CK
  endif()
endif()

# ---- Dependencies ----

include(${Catch2_SOURCE_DIR}/extras/Catch.cmake)

# Provide a simple smoke test to make sure that the CLI works and can display a --help message
add_test(NAME cli.has_help COMMAND watpocket --help)

# Provide a test to verify that the version being reported from the application
# matches the version given to CMake. This will be important once you package
# your program. Real world shows that this is the kind of simple mistake that is easy
# to make, but also easy to test for.
add_test(NAME cli.version_matches COMMAND watpocket --version)
set_tests_properties(cli.version_matches PROPERTIES PASS_REGULAR_EXPRESSION "${PROJECT_VERSION}")

get_filename_component(WATPOCKET_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(WATPOCKET_WCN_DATA_DIR "${WATPOCKET_SOURCE_ROOT}/tests/data/wcn")
if(EXISTS "${WATPOCKET_WCN_DATA_DIR}/complex_wcn.pdb")
  add_test(
    NAME cli.draw_rejects_unknown_extension
    COMMAND
      watpocket
      "${WATPOCKET_WCN_DATA_DIR}/complex_wcn.pdb"
      --resnums
      164,128,160,55
      --draw
      "${CMAKE_CURRENT_BINARY_DIR}/draw_mode.txt")
  set_tests_properties(
    cli.draw_rejects_unknown_extension
    PROPERTIES PASS_REGULAR_EXPRESSION "--draw output path must end with .py or .pdb")

  add_test(
    NAME cli.draw_writes_hull_pdb
    COMMAND
      ${CMAKE_COMMAND}
      -DWATPOCKET_BIN=$<TARGET_FILE:watpocket>
      -DINPUT1=${WATPOCKET_WCN_DATA_DIR}/complex_wcn.pdb
      -DRESNUMS=164,128,160,55,167,61,42,65,66
      -DDRAW_OUT=${CMAKE_CURRENT_BINARY_DIR}/draw_mode.pdb
      -P
      ${CMAKE_CURRENT_SOURCE_DIR}/verify_draw_pdb_contains_waters.cmake)
endif()

if(EXISTS "${WATPOCKET_WCN_DATA_DIR}/complex_wcn.parm7" AND EXISTS "${WATPOCKET_WCN_DATA_DIR}/complex_wcn_0.00000.nc")
  add_test(
    NAME cli.trajectory_draw_writes_model_pdb
    COMMAND
      ${CMAKE_COMMAND}
      -DWATPOCKET_BIN=$<TARGET_FILE:watpocket>
      -DINPUT1=${WATPOCKET_WCN_DATA_DIR}/complex_wcn.parm7
      -DINPUT2=${WATPOCKET_WCN_DATA_DIR}/complex_wcn_0.00000.nc
      -DRESNUMS=164,128,160,55,167,61,42,65,66
      -DCSV_OUT=${CMAKE_CURRENT_BINARY_DIR}/trajectory_mode_draw.csv
      -DDRAW_OUT=${CMAKE_CURRENT_BINARY_DIR}/trajectory_mode_draw.pdb
      -DREQUIRE_MODEL=ON
      -P
      ${CMAKE_CURRENT_SOURCE_DIR}/verify_draw_pdb_contains_waters.cmake)

  add_test(
    NAME cli.trajectory_draw_rejects_py
    COMMAND
      watpocket
      "${WATPOCKET_WCN_DATA_DIR}/complex_wcn.parm7"
      "${WATPOCKET_WCN_DATA_DIR}/complex_wcn_0.00000.nc"
      --resnums
      164,128,160,55
      -o
      "${CMAKE_CURRENT_BINARY_DIR}/trajectory_mode_draw_py.csv"
      --draw
      "${CMAKE_CURRENT_BINARY_DIR}/trajectory_mode_draw.py")
  set_tests_properties(
    cli.trajectory_draw_rejects_py
    PROPERTIES PASS_REGULAR_EXPRESSION "trajectory mode --draw output path must end with .pdb")

  add_test(
    NAME cli.trajectory_requires_output
    COMMAND
      watpocket
      "${WATPOCKET_WCN_DATA_DIR}/complex_wcn.parm7"
      "${WATPOCKET_WCN_DATA_DIR}/complex_wcn_0.00000.nc"
      --resnums
      164,128,160,55)
  set_tests_properties(
    cli.trajectory_requires_output
    PROPERTIES PASS_REGULAR_EXPRESSION "-o/--output is required in trajectory mode")

  add_test(
    NAME cli.parm7_requires_two_inputs
    COMMAND watpocket "${WATPOCKET_WCN_DATA_DIR}/complex_wcn.parm7" --resnums 164,128,160,55)
  set_tests_properties(
    cli.parm7_requires_two_inputs
    PROPERTIES PASS_REGULAR_EXPRESSION
               "parm7/prmtop topology files are only supported in trajectory mode")

  add_test(
    NAME cli.parm7_chain_selector_without_chainid_fails
    COMMAND
      watpocket
      "${WATPOCKET_WCN_DATA_DIR}/complex_wcn.parm7"
      "${WATPOCKET_WCN_DATA_DIR}/complex_wcn_0.00000.nc"
      --resnums
      A:164,128,160,55
      -o
      "${CMAKE_CURRENT_BINARY_DIR}/chain_selector_without_chainid.csv")
  set_tests_properties(
    cli.parm7_chain_selector_without_chainid_fails
    PROPERTIES PASS_REGULAR_EXPRESSION "has no RESIDUE_CHAINID section")
endif()

add_executable(tests tests.cpp)
target_link_libraries(
  tests
  PRIVATE myproject::myproject_warnings
          myproject::myproject_options
          myproject::sample_library
          Catch2::Catch2WithMain)

myproject_configure_linker(tests)

if(WIN32 AND BUILD_SHARED_LIBS)
  add_custom_command(
    TARGET tests
    PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:tests> $<TARGET_FILE_DIR:tests>
    COMMAND_EXPAND_LISTS)
endif()

# automatically discover tests that are defined in catch based test files you can modify the unittests. Set TEST_PREFIX
# to whatever you want, or use different for different binaries
catch_discover_tests(
  tests
  TEST_PREFIX
  "unittests."
  DISCOVERY_MODE
  PRE_TEST
  REPORTER
  XML
  OUTPUT_DIR
  .
  OUTPUT_PREFIX
  "unittests."
  OUTPUT_SUFFIX
  .xml)

# Add a file containing a set of constexpr tests
add_executable(constexpr_tests constexpr_tests.cpp)
target_link_libraries(
  constexpr_tests
  PRIVATE myproject::myproject_warnings
          myproject::myproject_options
          myproject::sample_library
          Catch2::Catch2WithMain)

myproject_configure_linker(constexpr_tests)

catch_discover_tests(
  constexpr_tests
  TEST_PREFIX
  "constexpr."
  DISCOVERY_MODE
  PRE_TEST
  REPORTER
  XML
  OUTPUT_DIR
  .
  OUTPUT_PREFIX
  "constexpr."
  OUTPUT_SUFFIX
  .xml)

# Disable the constexpr portion of the test, and build again this allows us to have an executable that we can debug when
# things go wrong with the constexpr testing
add_executable(relaxed_constexpr_tests constexpr_tests.cpp)
target_link_libraries(
  relaxed_constexpr_tests
  PRIVATE myproject::myproject_warnings
          myproject::myproject_options
          myproject::sample_library
          Catch2::Catch2WithMain)
target_compile_definitions(relaxed_constexpr_tests PRIVATE -DCATCH_CONFIG_RUNTIME_STATIC_REQUIRE)

myproject_configure_linker(relaxed_constexpr_tests)


catch_discover_tests(
  relaxed_constexpr_tests
  TEST_PREFIX
  "relaxed_constexpr."
  DISCOVERY_MODE
  PRE_TEST
  REPORTER
  XML
  OUTPUT_DIR
  .
  OUTPUT_PREFIX
  "relaxed_constexpr."
  OUTPUT_SUFFIX
  .xml)
