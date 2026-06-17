add_executable(smoke smoke.cpp)
target_link_libraries(smoke PRIVATE bench)
add_test(NAME smoke COMMAND smoke)

add_executable(util util.cpp)
target_link_libraries(util PRIVATE bench)
add_test(NAME util COMMAND util)
