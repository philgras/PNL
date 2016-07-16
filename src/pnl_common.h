/*
 * pnl_common.h
 *
 *  Created on: 19.06.2016
 *      Author: philgras
 */

#ifndef SRC_PNL_COMMON_H_
#define SRC_PNL_COMMON_H_

#include "pnl_error.h"
#include "pnl_time.h"
#include <stdlib.h>
/*
* MEMORY
 */
#define pnl_malloc(s) malloc((s))
#define pnl_realloc(ptr,s) realloc((ptr),(s))
#define pnl_free(ptr) free((ptr))

#include "pnl_buffer.h"

/*
 * FILE DESCRIPTORS
 */
#define INVALID_FD  -1
typedef int pnl_fd_t;

#endif /* SRC_PNL_COMMON_H_ */
