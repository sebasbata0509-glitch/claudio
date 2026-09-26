add_executable(nutsweller_render render.cpp)
target_link_libraries(nutsweller_render PRIVATE nutsweller_dsp)
target_include_directories(nutsweller_render PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)

add_executable(nutsweller_bench bench.cpp)
target_link_libraries(nutsweller_bench PRIVATE nutsweller_dsp)
target_include_directories(nutsweller_bench PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
