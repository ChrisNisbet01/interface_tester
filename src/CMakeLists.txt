cmake_minimum_required(VERSION 3.26)

set(CMAKE_C_STANDARD 23)

set(EXE_NAME interface_tester)

option(DEBUG "Include debug output" OFF)
option(METRICS_ADJUSTMENT "Include support for adjusting metrics (requires netifd support)" OFF)

add_compile_options(
        -std=gnu11
        -O3
        -Wall
        -Wextra
        -Werror
        -D_GNU_SOURCE
)

# Convert the boolean options to 0 or 1
set(DEBUG_VALUE 0)
if(DEBUG)
  set(DEBUG_VALUE 1)
  add_compile_options(-g)
endif()

set(METRICS_ADJUSTMENT_VALUE 0)
if(METRICS_ADJUSTMENT)
  set(METRICS_ADJUSTMENT_VALUE 1)
endif()

# Configure the config.h file
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/configure.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/configure.h
  @ONLY
)

# Include the directory with the generated config.h
include_directories(${CMAKE_CURRENT_BINARY_DIR})


find_library(BLOBMSG_JSON blobmsg_json REQUIRED)
find_library(JSON_C json-c REQUIRED)
find_library(UBOX ubox REQUIRED)
find_library(UBUS ubus REQUIRED)

# Function to print build options
function(print_build_options)
    message(STATUS "Current build options:")
    get_cmake_property(cache_variables CACHE_VARIABLES)
    foreach (variable ${cache_variables})
        get_property(current_helpstring CACHE "${variable}" PROPERTY HELPSTRING)
        if (current_helpstring)
            get_property(current_type CACHE "${variable}" PROPERTY TYPE)
            if("${current_type}" STREQUAL "BOOL")
                message(STATUS "  ${variable}: ${${variable}} (${current_helpstring})")
            endif()
        endif()
    endforeach()
endfunction()

# Call the function to print the options
print_build_options()

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
