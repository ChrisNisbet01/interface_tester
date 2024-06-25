cmake_minimum_required(VERSION 3.26)
project(interface_tester C)


add_subdirectory(src)
add_subdirectory(configurator)
add_subdirectory(/home/chris/projects/json-c json-c EXCLUDE_FROM_ALL)
set (BUILD_LUA NO)
add_subdirectory(/home/chris/projects/libubox libubox EXCLUDE_FROM_ALL)
add_subdirectory(/home/chris/projects/ubus ubus EXCLUDE_FROM_ALL)

