add_executable(nutsweller_render render.cpp)
target_link_libraries(nutsweller_render PRIVATE nutsweller_dsp)
target_include_directories(nutsweller_render PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
