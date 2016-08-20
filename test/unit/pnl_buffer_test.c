/*
 * pnl_buffer_test.c
 *
 *  Created on: 19.06.2016
 *      Author: philgras
 */

#include "pnl_buffer.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#define BUFFERSIZE 10

#define ASSERT_EQ(v1,v2) if(v1 != v2) exit(EXIT_FAILURE)

int main(void){

	pnl_buffer_t buf;
	int rc;

	rc = pnl_buffer_alloc(&buf, BUFFERSIZE );
	ASSERT_EQ(rc,PNL_OK);

	//test read and write operations

	pnl_buffer_free(&buf);

    return EXIT_SUCCESS;
}

