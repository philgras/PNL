/*
 * pnl_error.c
 *
 *  Created on: 19.06.2016
 *      Author: philgras
 */

#include "pnl_error.h"
#include <string.h>
#include <netdb.h>
#include <pnl_error.h>

static const char *error_msg[] =
        {
#define CC(ec, desc) desc
        PNL_ERROR_MAP(CC)
#undef CC
        };


const char *pnl_str_pnl_error(const pnl_error_t *error) {
    if (error->pnl_ec < 0 || error->pnl_ec >= sizeof(error_msg)) return NULL;
    return error_msg[error->pnl_ec];
}

const char *pnl_str_system_error(const pnl_error_t *error) {
    if (error->pnl_ec == PNL_EADDRINFO) return gai_strerror(error->system_ec);
    else return strerror(error->system_ec);
}

