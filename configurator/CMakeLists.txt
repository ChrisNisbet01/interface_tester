cmake_minimum_required(VERSION 3.26)

set(CMAKE_C_STANDARD 23)

set(EXE_NAME interface_configurator)

option(DEBUG "Include debug output" ON)

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
        configurator.h
        debug.h
        event_processor.c
        event_processor.h
        process.c
        process.h
        ubus.c
        ubus.h
        utils.h
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
