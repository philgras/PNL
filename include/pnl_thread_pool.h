/**
 * Created by pgrassal on 28.07.17.
 */

#ifndef PNL_PNL_THREAD_POOL_H
#define PNL_PNL_THREAD_POOL_H

#include <stddef.h>

struct pnl_thread_pool_s;

typedef struct pnl_thread_pool_s pnl_thread_pool_t;

typedef int (*job_routine)(void* data);


/**
 * Creates a thread pool managing the given number of threads
 * @param number_of_threads the number of threads to create
 * @return a valid pointer to a newly allocated and initialized thread pool or NULL if an error occurred
 */
pnl_thread_pool_t* pnl_thread_pool_create(size_t number_of_threads);


/**
 * adds a new job to the internal job queue which will be processed as soon as there is an idle thread
 * @param pool
 * @param routine work to be done by one thread
 * @param data data to be passed to routine
 * @return PNL_ERROR or PNL_OK
 */
int pnl_thread_pool_schedule_job(pnl_thread_pool_t* pool, job_routine routine, void* data);


/**
 * Waits for all threads to finish jobs they have been assigned to. In addition, the internal job queue will be closed,
 * thus rejecting future request to add jobs.
 * @param pool
 */
void pnl_thread_pool_finish(pnl_thread_pool_t *pool);


/**
 * Kills all threads if they have not already finished and frees all thread pool resources
 * @param pool the pool to stop
 */
void pnl_thread_pool_free(pnl_thread_pool_t *pool);


#endif /*PNL_PNL_THREAD_POOL_H*/
