add_executable(watpocket main.cpp)

target_link_libraries(
  watpocket
  PRIVATE myproject::myproject_options
          myproject::myproject_warnings
          chemfiles
          CGAL::CGAL)

target_link_system_libraries(
  watpocket
  PRIVATE
          CLI11::CLI11)

target_include_directories(watpocket PRIVATE "${CMAKE_BINARY_DIR}/configured_files/include")

myproject_configure_linker(watpocket)
