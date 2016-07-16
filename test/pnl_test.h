/*
 * pnl_test.h
 *
 *  Created on: 16.07.2016
 *      Author: philgras
 */

#ifndef TEST_PNL_TEST_H_
#define TEST_PNL_TEST_H_

#include <stdio.h>

#define PNL_TEST_CASE

#define PNL_SETUP(test_name) void test_name ## _setup(void ** test_data)

#define PNL_CLEANUP
#define PNL_TEST(test_name) 										\
																						\
int main(void){																	\
	test_name ## _setup();													\
																						\
	test_name ## _cleanup();												\
}





#endif /* TEST_PNL_TEST_H_ */
