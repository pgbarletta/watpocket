if(NOT DEFINED WATPOCKET_BIN OR WATPOCKET_BIN STREQUAL "")
  message(FATAL_ERROR "WATPOCKET_BIN is required")
endif()

if(NOT DEFINED RUN_SCRIPT OR RUN_SCRIPT STREQUAL "")
  message(FATAL_ERROR "RUN_SCRIPT is required")
endif()

if(NOT DEFINED DATA_DIR OR DATA_DIR STREQUAL "")
  message(FATAL_ERROR "DATA_DIR is required")
endif()

if(NOT DEFINED BENCHMARK_DIR OR BENCHMARK_DIR STREQUAL "")
  message(FATAL_ERROR "BENCHMARK_DIR is required")
endif()

if(NOT DEFINED OUTPUT_DIR OR OUTPUT_DIR STREQUAL "")
  message(FATAL_ERROR "OUTPUT_DIR is required")
endif()

if(NOT EXISTS "${WATPOCKET_BIN}")
  message(FATAL_ERROR "WATPOCKET_BIN does not exist: ${WATPOCKET_BIN}")
endif()

if(NOT EXISTS "${RUN_SCRIPT}")
  message(FATAL_ERROR "RUN_SCRIPT does not exist: ${RUN_SCRIPT}")
endif()

if(NOT IS_DIRECTORY "${DATA_DIR}")
  message(FATAL_ERROR "DATA_DIR is not a directory: ${DATA_DIR}")
endif()

if(NOT IS_DIRECTORY "${BENCHMARK_DIR}")
  message(FATAL_ERROR "BENCHMARK_DIR is not a directory: ${BENCHMARK_DIR}")
endif()

file(MAKE_DIRECTORY "${OUTPUT_DIR}")

execute_process(
  COMMAND bash "${RUN_SCRIPT}" "${WATPOCKET_BIN}" "${DATA_DIR}" "${OUTPUT_DIR}"
  RESULT_VARIABLE run_result
  OUTPUT_VARIABLE run_stdout
  ERROR_VARIABLE run_stderr)

if(NOT run_result EQUAL 0)
  message(
    FATAL_ERROR
      "Integration run failed with exit code ${run_result}\nSTDOUT:\n${run_stdout}\nSTDERR:\n${run_stderr}")
endif()

set(expected_scripts 0complex_wcn.py 1complex_wcn.py)

foreach(script_name IN LISTS expected_scripts)
  set(generated_script "${OUTPUT_DIR}/${script_name}")
  set(benchmark_script "${BENCHMARK_DIR}/${script_name}")

  if(NOT EXISTS "${generated_script}")
    message(FATAL_ERROR "Generated output is missing: ${generated_script}")
  endif()

  if(NOT EXISTS "${benchmark_script}")
    message(FATAL_ERROR "Benchmark output is missing: ${benchmark_script}")
  endif()

  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${generated_script}" "${benchmark_script}"
    RESULT_VARIABLE compare_result)

  if(NOT compare_result EQUAL 0)
    message(
      FATAL_ERROR
        "Generated output differs from benchmark: ${script_name}\nGenerated: ${generated_script}\nBenchmark: ${benchmark_script}")
  endif()
endforeach()
