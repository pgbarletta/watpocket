if(NOT DEFINED WATPOCKET_BIN OR WATPOCKET_BIN STREQUAL "")
  message(FATAL_ERROR "WATPOCKET_BIN is required")
endif()

if(NOT DEFINED TOPOLOGY_PATH OR TOPOLOGY_PATH STREQUAL "")
  message(FATAL_ERROR "TOPOLOGY_PATH is required")
endif()

if(NOT DEFINED TRAJECTORY_PATH OR TRAJECTORY_PATH STREQUAL "")
  message(FATAL_ERROR "TRAJECTORY_PATH is required")
endif()

if(NOT DEFINED RESNUMS OR RESNUMS STREQUAL "")
  message(FATAL_ERROR "RESNUMS is required")
endif()

if(NOT DEFINED BENCHMARK_CSV OR BENCHMARK_CSV STREQUAL "")
  message(FATAL_ERROR "BENCHMARK_CSV is required")
endif()

if(NOT DEFINED OUTPUT_CSV OR OUTPUT_CSV STREQUAL "")
  message(FATAL_ERROR "OUTPUT_CSV is required")
endif()

if(NOT EXISTS "${WATPOCKET_BIN}")
  message(FATAL_ERROR "WATPOCKET_BIN does not exist: ${WATPOCKET_BIN}")
endif()

if(NOT EXISTS "${TOPOLOGY_PATH}")
  message(FATAL_ERROR "TOPOLOGY_PATH does not exist: ${TOPOLOGY_PATH}")
endif()

if(NOT EXISTS "${TRAJECTORY_PATH}")
  message(FATAL_ERROR "TRAJECTORY_PATH does not exist: ${TRAJECTORY_PATH}")
endif()

if(NOT EXISTS "${BENCHMARK_CSV}")
  message(FATAL_ERROR "BENCHMARK_CSV does not exist: ${BENCHMARK_CSV}")
endif()

get_filename_component(output_dir "${OUTPUT_CSV}" DIRECTORY)
file(MAKE_DIRECTORY "${output_dir}")

execute_process(
  COMMAND "${WATPOCKET_BIN}" "${TOPOLOGY_PATH}" "${TRAJECTORY_PATH}" --resnums "${RESNUMS}" -o "${OUTPUT_CSV}"
  RESULT_VARIABLE run_result
  OUTPUT_VARIABLE run_stdout
  ERROR_VARIABLE run_stderr)

if(NOT run_result EQUAL 0)
  message(
    FATAL_ERROR
      "watpocket run failed with exit code ${run_result}\nSTDOUT:\n${run_stdout}\nSTDERR:\n${run_stderr}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E compare_files "${OUTPUT_CSV}" "${BENCHMARK_CSV}"
  RESULT_VARIABLE compare_result)

if(NOT compare_result EQUAL 0)
  message(
    FATAL_ERROR
      "Generated CSV differs from benchmark\nGenerated: ${OUTPUT_CSV}\nBenchmark: ${BENCHMARK_CSV}")
endif()
