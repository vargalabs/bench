add_executable(smoke smoke.cpp)
target_link_libraries(smoke PRIVATE bench)
add_test(NAME smoke COMMAND smoke)

add_executable(type_dispatch type_dispatch.cpp)
target_link_libraries(type_dispatch PRIVATE bench)
add_test(NAME type_dispatch COMMAND type_dispatch)
