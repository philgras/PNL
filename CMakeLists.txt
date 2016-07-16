cmake_minimum_required (VERSION 2.6 FATAL_ERROR)
project (PNL C)

if(CMAKE_C_COMPILER_ID MATCHES GNU)
	set(CMAKE_C_FLAGS "-Wall -Wfatal-errors -pedantic -O0 -g3 -fstrict-aliasing -std=c99 -D_POSIX_SOURCE -D_GNU_SOURCE")
	set(CMAKE_C_FLAGS_DEBUG "-O0 -g3")
	set(CMAKE_C_FLAGS_RELEASE "-O3")

endif()

include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_SOURCE_DIR}/test)

set(CMAKE_BINARY_DIR "bin")
enable_testing()
add_library(pnl_base src/pnl_error.c src/pnl_time.c)
add_library(pnl_tcp src/pnl_tcp.c)
add_library(pnl_loop src/pnl_loop.c)
target_link_libraries(pnl_loop pnl_tcp pnl_base)


add_executable(pnl_list_test test/pnl_list_test.c)


add_executable(pnl_server_test test/pnl_server_test.c)
target_link_libraries(pnl_server_test pnl_loop)


add_executable(pnl_client_test test/pnl_client_test.c)
target_link_libraries(pnl_client_test pnl_loop)

#add_executable(pnl_tcp_listen_test test/pnl_tcp_listen_test.c src/pnl_tcp.c)
#target_link_libraries(pnl_tcp_listen_test pnl_base)

#add_executable(pnl_connect_test test/pnl_connect_test.c src/pnl_tcp.c)
#target_link_libraries(pnl_connect_test pnl_base)

add_test(NAME pnl_list_test COMMAND pnl_list_test)
