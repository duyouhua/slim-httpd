cmake_minimum_required (VERSION 2.6)

project (slim-httpd C)

include(${CMAKE_ROOT}/Modules/CheckFunctionExists.cmake)

set (POLLER "")

if (POLLER STREQUAL "")
    set (CMAKE_REQUIRED_INCLUDES sys/event.h)
    check_function_exists(kqueue HAVE_KQUEUE) 
    
    if (HAVE_KQUEUE)
        set(POLLER "kqueue")
    endif()
endif()

if (POLLER STREQUAL "")
    set (CMAKE_REQUIRED_INCLUDES sys/epoll.h)
    check_function_exists(epoll_create HAVE_EPOLL) 
    
    if (HAVE_KQUEUE)
        set(POLLER "epoll")
    endif()    
endif()

configure_file(
    "${PROJECT_SOURCE_DIR}/slim-httpd.h.in"
    "${PROJECT_BINARY_DIR}/slim-httpd.h"
)

set (SRC
    zyzi_log.c
    zyzi_http.c
    zyzi_event.c
    zyzi_malloc.c
)
add_library(slim-httpd-base ${SRC})

add_executable(slim-httpd slim-httpd.c)

target_link_libraries(slim-httpd slim-httpd-base)