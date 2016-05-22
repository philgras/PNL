cmake_minimum_required (VERSION 2.6 FATAL_ERROR)
project (PNL C)

if(CMAKE_C_COMPILER_ID MATCHES GNU)
	set(CMAKE_C_FLAGS "-Wall -Wfatal-errors -pedantic -std=c99")
	set(CMAKE_C_FLAGS_DEBUG "-O0 -g3")
	set(CMAKE_C_FLAGS_RELEASE "-O3")

endif()

include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_SOURCE_DIR}/test)

set(CMAKE_BINARY_DIR "bin")
enable_testing()


add_executable(pnl_list_test test/pnl_list_test.c)

add_test(NAME pnl_list_test COMMAND pnl_list_test)
