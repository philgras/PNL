/*
 * pnl_time.c
 *
 *  Created on: 19.06.2016
 *      Author: philgras
 */

#include "pnl_time.h"
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#define NANO_TO_MILLI(nano) ((nano)/1000000)
#define SEC_TO_MILLI(sec) ((sec)*1000)

pnl_time_t pnl_get_system_time(){
	struct timespec time;
	int rc = clock_gettime(CLOCK_MONOTONIC,&time);
	if(rc != 0){
		fprintf(stderr,"Unable to determine the system time...\n");
		exit(EXIT_FAILURE);
	}
	return  SEC_TO_MILLI(time.tv_sec) +   NANO_TO_MILLI(time.tv_nsec);
}
