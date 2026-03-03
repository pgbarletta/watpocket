cmake_minimum_required(VERSION 3.15...3.23)

project(CmakeConfigPackageTests 
  VERSION ${PROJECT_VERSION}
  LANGUAGES CXX)

# ---- Test as standalone project the exported config package ----

if(PROJECT_IS_TOP_LEVEL OR TEST_INSTALLED_VERSION)
  enable_testing()

  find_package(watpocket CONFIG REQUIRED) # for watpocket library + CLI

  if(NOT TARGET watpocket::watpocket)
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
set(WATPOCKET_WCN_DATA_DIR "${WATPOCKET_SOURCE_ROOT}/test/data/wcn")
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

if(EXISTS "${WATPOCKET_WCN_DATA_DIR}/0complex_wcn.pdb"
   AND EXISTS "${WATPOCKET_WCN_DATA_DIR}/1complex_wcn.pdb"
   AND EXISTS "${WATPOCKET_WCN_DATA_DIR}/0complex_wcn.py"
   AND EXISTS "${WATPOCKET_WCN_DATA_DIR}/1complex_wcn.py")
  add_test(
    NAME integration.wcn.pymol_scripts_match_benchmark
    COMMAND
      ${CMAKE_COMMAND}
      -DWATPOCKET_BIN=$<TARGET_FILE:watpocket>
      -DRUN_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/integration/wcn/run.sh
      -DDATA_DIR=${WATPOCKET_WCN_DATA_DIR}
      -DBENCHMARK_DIR=${WATPOCKET_WCN_DATA_DIR}
      -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/integration/wcn
      -P
      ${CMAKE_CURRENT_SOURCE_DIR}/integration/wcn/verify_outputs_match_benchmarks.cmake)
endif()

if(EXISTS "${WATPOCKET_WCN_DATA_DIR}/complex_wcn.parm7"
   AND EXISTS "${WATPOCKET_WCN_DATA_DIR}/0.nc"
   AND EXISTS "${WATPOCKET_WCN_DATA_DIR}/1.nc"
   AND EXISTS "${WATPOCKET_WCN_DATA_DIR}/0.nc.csv"
   AND EXISTS "${WATPOCKET_WCN_DATA_DIR}/1.nc.csv")
  add_test(
    NAME integration.wcn.trajectory_0_csv_matches_benchmark
    COMMAND
      ${CMAKE_COMMAND}
      -DWATPOCKET_BIN=$<TARGET_FILE:watpocket>
      -DTOPOLOGY_PATH=${WATPOCKET_WCN_DATA_DIR}/complex_wcn.parm7
      -DTRAJECTORY_PATH=${WATPOCKET_WCN_DATA_DIR}/0.nc
      -DRESNUMS=164,128,160,55,167,61,42,65,66
      -DBENCHMARK_CSV=${WATPOCKET_WCN_DATA_DIR}/0.nc.csv
      -DOUTPUT_CSV=${CMAKE_CURRENT_BINARY_DIR}/integration/wcn/0.nc.csv
      -P
      ${CMAKE_CURRENT_SOURCE_DIR}/integration/wcn/verify_trajectory_csv_matches_benchmark.cmake)

  add_test(
    NAME integration.wcn.trajectory_1_csv_matches_benchmark
    COMMAND
      ${CMAKE_COMMAND}
      -DWATPOCKET_BIN=$<TARGET_FILE:watpocket>
      -DTOPOLOGY_PATH=${WATPOCKET_WCN_DATA_DIR}/complex_wcn.parm7
      -DTRAJECTORY_PATH=${WATPOCKET_WCN_DATA_DIR}/1.nc
      -DRESNUMS=164,128,160,55,167,61,42,65,66
      -DBENCHMARK_CSV=${WATPOCKET_WCN_DATA_DIR}/1.nc.csv
      -DOUTPUT_CSV=${CMAKE_CURRENT_BINARY_DIR}/integration/wcn/1.nc.csv
      -P
      ${CMAKE_CURRENT_SOURCE_DIR}/integration/wcn/verify_trajectory_csv_matches_benchmark.cmake)
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
    NAME cli.trajectory_threads_zero_fails
    COMMAND
      watpocket
      "${WATPOCKET_WCN_DATA_DIR}/complex_wcn.parm7"
      "${WATPOCKET_WCN_DATA_DIR}/complex_wcn_0.00000.nc"
      --resnums
      164,128,160,55
      -o
      "${CMAKE_CURRENT_BINARY_DIR}/threads_zero.csv"
      --threads
      0)
  set_tests_properties(
    cli.trajectory_threads_zero_fails
    PROPERTIES PASS_REGULAR_EXPRESSION "num_threads must be >= 1")

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

if(TARGET watpocket_ext
   AND EXISTS "${WATPOCKET_WCN_DATA_DIR}/complex_wcn.parm7"
   AND EXISTS "${WATPOCKET_WCN_DATA_DIR}/0.nc"
   AND EXISTS "${WATPOCKET_WCN_DATA_DIR}/0.nc.csv"
   AND EXISTS "${WATPOCKET_WCN_DATA_DIR}/1.nc"
   AND EXISTS "${WATPOCKET_WCN_DATA_DIR}/1.nc.csv")
  find_package(Python3 3.10 COMPONENTS Interpreter REQUIRED)

  add_test(
    NAME integration.python.wcn_trajectory_0nc_threads
    COMMAND
      ${CMAKE_COMMAND}
      -E
      env
      "PYTHONPATH=$<TARGET_FILE_DIR:watpocket_ext>"
      "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/python/integration_wcn_0nc_threads.py"
      --topology
      "${WATPOCKET_WCN_DATA_DIR}/complex_wcn.parm7"
      --trajectory
      "${WATPOCKET_WCN_DATA_DIR}/0.nc"
      --benchmark
      "${WATPOCKET_WCN_DATA_DIR}/0.nc.csv"
      --resnums
      "164,128,160,55,167,61,42,65,66")

  add_test(
    NAME integration.python.wcn_trajectory_1nc
    COMMAND
      ${CMAKE_COMMAND}
      -E
      env
      "PYTHONPATH=$<TARGET_FILE_DIR:watpocket_ext>"
      "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/python/integration_wcn_1nc.py"
      --topology
      "${WATPOCKET_WCN_DATA_DIR}/complex_wcn.parm7"
      --trajectory
      "${WATPOCKET_WCN_DATA_DIR}/1.nc"
      --benchmark
      "${WATPOCKET_WCN_DATA_DIR}/1.nc.csv"
      --resnums
      "164,128,160,55,167,61,42,65,66")
endif()

add_executable(watpocket_api_tests watpocket_api_tests.cpp)
target_link_libraries(
  watpocket_api_tests
  PRIVATE myproject::myproject_warnings
          myproject::myproject_options
          watpocket_lib
          chemfiles
          Catch2::Catch2WithMain)
target_compile_definitions(watpocket_api_tests PRIVATE WATPOCKET_SOURCE_ROOT="${WATPOCKET_SOURCE_ROOT}")

myproject_configure_linker(watpocket_api_tests)

catch_discover_tests(
  watpocket_api_tests
  TEST_PREFIX
  "api."
  DISCOVERY_MODE
  PRE_TEST
  REPORTER
  XML
  OUTPUT_DIR
  .
  OUTPUT_PREFIX
  "api."
  OUTPUT_SUFFIX
  .xml)

# Ensure the exported CMake package target can be used by a "consumer" translation unit
# that only includes watpocket headers (no Chemfiles/CGAL headers).
add_executable(watpocket_package_consumer package_consumer.cpp)
target_link_libraries(
  watpocket_package_consumer
  PRIVATE watpocket::watpocket)
add_test(NAME package.consumer_runs COMMAND watpocket_package_consumer)
