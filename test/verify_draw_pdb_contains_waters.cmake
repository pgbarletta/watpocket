if(NOT DEFINED WATPOCKET_BIN OR WATPOCKET_BIN STREQUAL "")
  message(FATAL_ERROR "WATPOCKET_BIN is required")
endif()

if(NOT DEFINED INPUT1 OR INPUT1 STREQUAL "")
  message(FATAL_ERROR "INPUT1 is required")
endif()

if(NOT DEFINED RESNUMS OR RESNUMS STREQUAL "")
  message(FATAL_ERROR "RESNUMS is required")
endif()

if(NOT DEFINED DRAW_OUT OR DRAW_OUT STREQUAL "")
  message(FATAL_ERROR "DRAW_OUT is required")
endif()

if(NOT DEFINED REQUIRE_MODEL OR REQUIRE_MODEL STREQUAL "")
  set(REQUIRE_MODEL OFF)
endif()

if(DEFINED INPUT2 AND NOT INPUT2 STREQUAL "")
  if(NOT DEFINED CSV_OUT OR CSV_OUT STREQUAL "")
    message(FATAL_ERROR "CSV_OUT is required for trajectory mode")
  endif()

  execute_process(
    COMMAND
      "${WATPOCKET_BIN}"
      "${INPUT1}"
      "${INPUT2}"
      --resnums
      "${RESNUMS}"
      -o
      "${CSV_OUT}"
      --draw
      "${DRAW_OUT}"
    RESULT_VARIABLE run_result
    OUTPUT_VARIABLE run_stdout
    ERROR_VARIABLE run_stderr)
else()
  execute_process(
    COMMAND
      "${WATPOCKET_BIN}"
      "${INPUT1}"
      --resnums
      "${RESNUMS}"
      --draw
      "${DRAW_OUT}"
    RESULT_VARIABLE run_result
    OUTPUT_VARIABLE run_stdout
    ERROR_VARIABLE run_stderr)
endif()

if(NOT run_result EQUAL 0)
  message(
    FATAL_ERROR
      "watpocket command failed with exit code ${run_result}\nSTDOUT:\n${run_stdout}\nSTDERR:\n${run_stderr}")
endif()

if(NOT EXISTS "${DRAW_OUT}")
  message(FATAL_ERROR "Expected draw output file was not created: ${DRAW_OUT}")
endif()

file(READ "${DRAW_OUT}" pdb_text)

string(REGEX MATCH "ATOM[^\n]*(HOH|WAT|TIP3|TIP3P|SPC|SPCE)[^\n]*" water_match "${pdb_text}")
if(water_match STREQUAL "")
  message(FATAL_ERROR "No water ATOM records found in draw PDB output")
endif()

if(REQUIRE_MODEL)
  string(REGEX MATCH "MODEL[ ]+1" model_match "${pdb_text}")
  if(model_match STREQUAL "")
    message(FATAL_ERROR "Trajectory draw PDB is missing MODEL records")
  endif()

  string(REGEX MATCH "ENDMDL" endmdl_match "${pdb_text}")
  if(endmdl_match STREQUAL "")
    message(FATAL_ERROR "Trajectory draw PDB is missing ENDMDL records")
  endif()
endif()
