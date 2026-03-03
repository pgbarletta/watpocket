add_subdirectory(watpocket_lib)
add_subdirectory(watpocket)

if(WATPOCKET_ENABLE_PYTHON_BINDINGS)
  add_subdirectory(python)
endif()
