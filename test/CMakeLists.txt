add_executable(smoke smoke.cpp)
target_link_libraries(smoke PRIVATE bench)
add_test(NAME smoke COMMAND smoke)

add_executable(type_dispatch type_dispatch.cpp)
target_link_libraries(type_dispatch PRIVATE bench)
add_test(NAME type_dispatch COMMAND type_dispatch)

add_executable(util util.cpp)
target_link_libraries(util PRIVATE bench)
add_test(NAME util COMMAND util)

add_executable(sinks sinks.cpp)
target_link_libraries(sinks PRIVATE bench)
add_test(NAME sinks COMMAND sinks)

add_executable(svg svg.cpp)
target_link_libraries(svg PRIVATE bench)
add_test(NAME svg COMMAND svg)
