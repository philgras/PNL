#ifndef PNL_H
#define PNL_H

#include <stdlib.h>

#define pnl_malloc(s) malloc((s))
#define pnl_realloc(ptr,s) realloc((ptr),(s))
#define pnl_free(ptr) free((ptr))

#endif /*PNL_H*/
