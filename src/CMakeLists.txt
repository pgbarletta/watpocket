add_subdirectory(watpocket_lib)
if(WATPOCKET_BUILD_CLI)
  add_subdirectory(watpocket)
endif()

if(WATPOCKET_ENABLE_PYTHON_BINDINGS)
  add_subdirectory(python)
endif()
