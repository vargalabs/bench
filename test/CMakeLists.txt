add_executable(smoke smoke.cpp)
target_link_libraries(smoke PRIVATE bench)
add_test(NAME smoke COMMAND smoke)

add_executable(util util.cpp)
add_executable(sinks sinks.cpp)
target_link_libraries(sinks PRIVATE bench)
add_test(NAME sinks COMMAND sinks)
