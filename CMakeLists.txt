cmake_minimum_required (VERSION 2.6 FATAL_ERROR)
project (PNL C)

#compiler settings
if(CMAKE_C_COMPILER_ID MATCHES GNU)
	set(CMAKE_C_FLAGS "-Wall -Wfatal-errors -pedantic -O0 -g3 -fstrict-aliasing -std=c99 -D_POSIX_SOURCE -D_GNU_SOURCE")
	set(CMAKE_C_FLAGS_DEBUG "-O0 -g3")
	set(CMAKE_C_FLAGS_RELEASE "-O3")
endif()

#find and add the cmocka library
find_library(CMOCKA_LIBRARY NAMES cmocka)
add_library(cmocka SHARED IMPORTED)
set_property(TARGET cmocka PROPERTY IMPORTED_LOCATION "${CMOCKA_LIBRARY}")

#include the source directories
include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_SOURCE_DIR}/test)


set(CMAKE_BINARY_DIR "bin")
enable_testing()

add_library(pnl_base src/pnl_error.c src/pnl_time.c)
add_library(pnl_tcp src/pnl_tcp.c)
add_library(pnl_sys src/pnl_sys_nio.c)
add_library(pnl_loop src/pnl_loop.c)
target_link_libraries(pnl_loop pnl_base pnl_tcp pnl_sys)
#target_link_libraries(pnl_loop pnl_tcp pnl_base)

add_executable(pnl_demo_test test/unit/pnl_demo_test.c)
add_executable(pnl_client_test test/integration/pnl_client_test.c)
add_executable(pnl_server_test test/integration/pnl_server_test.c)
target_link_libraries(pnl_server_test pnl_loop)
target_link_libraries(pnl_client_test pnl_loop)
target_link_libraries(pnl_demo_test cmocka)

add_test(NAME pnl_demo_test COMMAND pnl_demo_test)
