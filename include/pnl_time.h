/*
 * pnl_time.h
 *
 *  Created on: 19.06.2016
 *      Author: philgras
 */

#ifndef SRC_PNL_TIME_H_
#define SRC_PNL_TIME_H_

#define PNL_INFINITE_TIMEOUT -1
#define PNL_DEFAULT_TIMEOUT 50000


typedef long pnl_time_t;

typedef struct {

    pnl_time_t last_action;
    pnl_time_t timeout;

} pnl_timer_t;

pnl_time_t pnl_get_system_time(void);

#endif /* SRC_PNL_TIME_H_ */
