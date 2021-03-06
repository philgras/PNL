cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
project(PNL C)

set(PNL_VERSION_MAJOR 0)
set(PNL_VERSION_MINOR 1)

#compiler settings
if (CMAKE_C_COMPILER_ID MATCHES GNU)
    set(CMAKE_C_FLAGS "-Wall -Wfatal-errors -pedantic -Wstrict-aliasing=2 -std=c99 -D_POSIX_SOURCE -D_GNU_SOURCE")
    set(CMAKE_C_FLAGS_DEBUG "-g3")
    set(CMAKE_C_FLAGS_RELEASE "-O3")
endif ()

#find threads
find_package(Threads REQUIRED)

#check if epoll and eventfd exist
include(CheckIncludeFiles)
check_include_files(sys/eventfd.h PNL_HAVE_EVENTFD)
check_include_files(sys/epoll.h PNL_HAVE_EPOLL)

if (NOT PNL_HAVE_EVENTFD)
    message(FATAL_ERROR "Required header sys/eventfd.h not found")
endif ()

if (NOT PNL_HAVE_EPOLL)
    message(FATAL_ERROR "Required header sys/epoll.h not found")
endif ()

#find and add the cmocka library
find_library(CMOCKA_LIBRARY NAMES cmocka)
if (CMOCKA_LIBRARY)
    add_library(cmocka SHARED IMPORTED)
    set_property(TARGET cmocka PROPERTY IMPORTED_LOCATION "${CMOCKA_LIBRARY}")

else ()
    include(ExternalProject)
    ExternalProject_Add(cmocka_lib
            DOWNLOAD_DIR ${CMAKE_SOURCE_DIR}/lib
            DOWNLOAD_COMMAND rm -rf ${CMAKE_SOURCE_DIR}/lib/*
            COMMAND wget https://cmocka.org/files/1.1/cmocka-1.1.0.tar.xz
            COMMAND tar -xf ${CMAKE_SOURCE_DIR}/lib/cmocka-1.1.0.tar.xz
            COMMAND rm ${CMAKE_SOURCE_DIR}/lib/cmocka-1.1.0.tar.xz
            COMMAND mv -T ${CMAKE_SOURCE_DIR}/lib/cmocka-1.1.0 ${CMAKE_SOURCE_DIR}/lib/cmocka
            SOURCE_DIR ${CMAKE_SOURCE_DIR}/lib/cmocka
            BUILD_COMMAND make
            INSTALL_DIR ${CMAKE_BINARY_DIR}/cmocka/install
            CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>)


    include_directories(${CMAKE_BINARY_DIR}/cmocka/install/include)
    add_library(cmocka SHARED IMPORTED)
    set_property(TARGET cmocka PROPERTY IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/cmocka/install/lib/libcmocka.so")
endif ()

#include the source directories
include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_SOURCE_DIR}/include)


set(CMAKE_BINARY_DIR "bin")
enable_testing()

add_library(pnl_base src/pnl_error.c src/pnl_time.c)
add_library(pnl_tcp src/pnl_tcp.c)
add_library(pnl_sys src/pnl_system.c)
add_library(pnl_loop src/pnl_loop.c)

#finally the library
add_library(pnl STATIC
        src/pnl_error.c
        src/pnl_time.c
        src/pnl_tcp.c
        src/pnl_system.c
        src/pnl_loop.c)

target_link_libraries(pnl ${CMAKE_THREAD_LIBS_INIT})

add_executable(pnl_demo_test test/unit/pnl_demo_test.c)
add_executable(pnl_list_test test/unit/pnl_list_test.c)
#add_executable(pnl_buffer_test test/unit/pnl_buffer_test.c)
add_executable(pnl_tcp_test test/unit/pnl_tcp_test.c)
add_executable(pnl_echo_test test/integration/pnl_echo_test.c)


target_link_libraries(pnl_demo_test cmocka)
target_link_libraries(pnl_list_test cmocka)
#target_link_libraries(pnl_buffer_test cmocka)
target_link_libraries(pnl_tcp_test pnl_tcp cmocka)
target_link_libraries(pnl_echo_test pnl cmocka)

add_test(NAME pnl_demo_test COMMAND pnl_demo_test)
add_test(NAME pnl_list_test COMMAND pnl_list_test)
#add_test(NAME pnl_buffer_test COMMAND pnl_buffer_test)
add_test(NAME pnl_tcp_test COMMAND pnl_tcp_test)
add_test(NAME pnl_echo_test COMMAND pnl_echo_test)

INSTALL(TARGETS pnl DESTINATION "lib") #/usr/local/lib
INSTALL(DIRECTORY ${PROJECT_SOURCE_DIR}/include/ DESTINATION "include/pnl")#include files
