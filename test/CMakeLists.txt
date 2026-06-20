add_executable(smoke smoke.cpp)
target_link_libraries(smoke PRIVATE bench)
add_test(NAME smoke COMMAND smoke)

add_executable(type_dispatch type_dispatch.cpp)
target_link_libraries(type_dispatch PRIVATE bench)
add_test(NAME type_dispatch COMMAND type_dispatch)

add_executable(metric_actions metric_actions.cpp)
target_link_libraries(metric_actions PRIVATE bench)
add_test(NAME metric_actions COMMAND metric_actions)

add_executable(util util.cpp)
target_link_libraries(util PRIVATE bench)
add_test(NAME util COMMAND util)

add_executable(sinks sinks.cpp)
target_link_libraries(sinks PRIVATE bench)
add_test(NAME sinks COMMAND sinks)

# The svg test exercises the opt-in <bench/sink/svg.hpp>, which needs the
# external plot:: library. Only build it when find_package(plot) succeeded.
if(plot_FOUND)
	add_executable(svg svg.cpp)
	target_link_libraries(svg PRIVATE bench plot::plot)
	add_test(NAME svg COMMAND svg)
else()
	message(STATUS "plot not found — skipping svg test")
endif()
