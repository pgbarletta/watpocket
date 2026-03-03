if(CMAKE_VERSION VERSION_LESS 3.18)
  set(PY_DEV_MODULE Development)
else()
  set(PY_DEV_MODULE Development.Module)
endif()

if(Python3_EXECUTABLE AND NOT Python_EXECUTABLE)
  set(Python_EXECUTABLE "${Python3_EXECUTABLE}" CACHE FILEPATH "Python executable for nanobind" FORCE)
endif()

find_package(Python 3.10 COMPONENTS Interpreter Development REQUIRED)

execute_process(
  COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
  OUTPUT_STRIP_TRAILING_WHITESPACE
  OUTPUT_VARIABLE nanobind_ROOT
  RESULT_VARIABLE nanobind_cmake_dir_result
  ERROR_VARIABLE nanobind_cmake_dir_stderr)

if(NOT nanobind_cmake_dir_result EQUAL 0)
  message(
    FATAL_ERROR
      "Failed to locate nanobind CMake package via Python (${Python_EXECUTABLE}). "
      "Install nanobind in this interpreter (e.g. `python -m pip install nanobind`). "
      "Details: ${nanobind_cmake_dir_stderr}")
endif()

find_package(nanobind CONFIG REQUIRED PATHS "${nanobind_ROOT}" NO_DEFAULT_PATH)

nanobind_add_module(watpocket_ext bindings.cpp)
target_link_libraries(watpocket_ext PRIVATE watpocket_lib)
target_compile_features(watpocket_ext PRIVATE cxx_std_${CMAKE_CXX_STANDARD})

set_target_properties(
  watpocket_ext
  PROPERTIES OUTPUT_NAME watpocket
             LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/python"
             RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/python"
             BUILD_RPATH "${CMAKE_BINARY_DIR}/src/watpocket_lib"
             INSTALL_RPATH "$ORIGIN/lib")

nanobind_add_stub(
  watpocket_stub
  MODULE
  watpocket
  OUTPUT
  watpocket.pyi
  PYTHON_PATH
  $<TARGET_FILE_DIR:watpocket_ext>
  DEPENDS
  watpocket_ext)

install(TARGETS watpocket_ext LIBRARY DESTINATION . RUNTIME DESTINATION .)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/watpocket.pyi" DESTINATION .)
