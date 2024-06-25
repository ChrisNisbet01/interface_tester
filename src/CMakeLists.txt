cmake_minimum_required(VERSION 3.26)

set(CMAKE_C_STANDARD 23)

set(EXE_NAME interface_tester)

option(DEBUG "Include debug output" OFF)

add_compile_options(
        -std=gnu11
        -O3
        -Wall
        -Wextra
        -Werror
        -D_GNU_SOURCE
)

if(DEBUG)
    add_compile_options(
        -DDEBUG=1
        -g
    )
endif()


find_library(BLOBMSG_JSON blobmsg_json REQUIRED)
find_library(JSON_C json-c REQUIRED)
find_library(UBOX ubox REQUIRED)
find_library(UBUS ubus REQUIRED)

add_executable(${EXE_NAME}
        main.c
        config.c
        config.h
        debug.h
        dump.c
        dump.h
        event_queue.c
        event_queue.h
        tester_common.c
        tester_common.h
        interface_connection.c
        interface_connection.h
        interface_tester.c
        interface_tester.h
        interface_tester_events.h
        process.c
        process.h
        shared.h
        strings.c
        strings.h
        ubus.c
        ubus.h
        utils.h
        timers.c
        timers.h
)

target_link_libraries(${EXE_NAME}
        ${BLOBMSG_JSON}
        ${UBOX}
        ${JSON_C}
        ${UBUS}
)

install(TARGETS ${EXE_NAME}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
