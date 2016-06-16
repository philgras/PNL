/*
 * pnl.c
 *
 *  Created on: 27.05.2016
 *      Author: philgras
 */

#include "pnl.h"
#include <time.h>

static const char* error_msg[] =
{
#define CC(ec,desc) desc
	PNL_ERROR_MAP(CC)
#undef CC
};


static const char* error_codes[] =
{
#define CC(ec,desc) #ec
	PNL_ERROR_MAP(CC)
#undef CC
};

const char* pnl_strerror(int ec){
	if(ec < 0) ec*=-1;
	if(ec >= sizeof(error_msg)) return NULL;
	return error_msg[ec];
}

const char* pnl_strrerrorcode(int ec){
	if(ec >= sizeof(error_codes)) return NULL;
	return error_codes[ec];
}

pnl_time_t pnl_get_system_time(){
	struct timespec time;
	int rc = clock_gettime(CLOCK_MONOTONIC,&time);
	return rc != 0? rc : time.tv_nsec/NANO_TO_MILLI;
}
