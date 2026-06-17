#  _________________________________________________________
#  Copyright (c) 2026 Steven Varga, Toronto, ON, Canada
#  MIT License — see LICENSE
#  _________________________________________________________
#
# Example executables. Plain programs that link bench::bench and demonstrate
# the library; they are NOT registered as ctest tests. Target names are
# prefixed `example_` so they never collide with the test targets that share
# the same source file names (test/type_dispatch, test/sinks).

add_executable(example_memcpy_bandwidth memcpy_bandwidth.cpp)
target_link_libraries(example_memcpy_bandwidth PRIVATE bench::bench)
set_target_properties(example_memcpy_bandwidth PROPERTIES OUTPUT_NAME memcpy_bandwidth)

add_executable(example_type_dispatch type_dispatch.cpp)
target_link_libraries(example_type_dispatch PRIVATE bench::bench)
set_target_properties(example_type_dispatch PROPERTIES OUTPUT_NAME type_dispatch)

add_executable(example_sinks sinks.cpp)
target_link_libraries(example_sinks PRIVATE bench::bench)
set_target_properties(example_sinks PROPERTIES OUTPUT_NAME sinks)

add_executable(example_heatmap heatmap.cpp)
target_link_libraries(example_heatmap PRIVATE bench::bench)
set_target_properties(example_heatmap PROPERTIES OUTPUT_NAME heatmap)

add_executable(example_svg_sink svg_sink.cpp)
target_link_libraries(example_svg_sink PRIVATE bench::bench)
set_target_properties(example_svg_sink PROPERTIES OUTPUT_NAME svg_sink)

add_executable(example_line_chart line_chart.cpp)
target_link_libraries(example_line_chart PRIVATE bench::bench)
set_target_properties(example_line_chart PROPERTIES OUTPUT_NAME line_chart)

add_executable(example_scatter scatter.cpp)
target_link_libraries(example_scatter PRIVATE bench::bench)
set_target_properties(example_scatter PROPERTIES OUTPUT_NAME scatter)
