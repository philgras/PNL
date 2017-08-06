/**
 * Created by pgrassal on 28.07.17.
 */
#include "pnl_thread_pool.h"
#include "pnl_list.h"
#include "pnl_common.h"

#include <pthread.h>

#include <stdatomic.h>
#include <stdlib.h>

#include <sys/sysinfo.h>

typedef struct {
    void *data;
    job_routine routine;
    pnl_list_t node;
} pnl_job_t;


typedef struct {

    pthread_t real_thread;
    /*pnl_job_t *job;*/

} pnl_thread_t;


struct pnl_thread_pool_s{

    size_t number_of_threads;

    /*PRIVATE*/
    pnl_thread_t *threads;
    pnl_list_t job_list;
    pthread_mutex_t list_mutex;
    pthread_cond_t list_cond;
    pthread_rwlock_t is_list_blocked_lock;
    unsigned char is_list_blocked:1;

};


static int initialize_pool(pnl_thread_pool_t *pool, size_t number_of_threads);

static void *main_worker_routine(void *thread_data);


static inline
void block_job_list(pnl_thread_pool_t *pool) {
    pthread_rwlock_wrlock(&pool->is_list_blocked_lock);
    pool->is_list_blocked = 1;
    pthread_rwlock_unlock(&pool->is_list_blocked_lock);
    pthread_cond_broadcast(&pool->list_cond);
}

static inline
unsigned char is_job_list_blocked(pnl_thread_pool_t *pool) {
    unsigned char is_list_blocked;
    pthread_rwlock_rdlock(&pool->is_list_blocked_lock);
    is_list_blocked = pool->is_list_blocked;
    pthread_rwlock_unlock(&pool->is_list_blocked_lock);
    return is_list_blocked;
}

static inline
void push_job_and_notify(pnl_thread_pool_t *pool, pnl_job_t *job) {
    pthread_mutex_lock(&pool->list_mutex);
    pnl_list_insert(&pool->job_list, &job->node);
    pthread_mutex_unlock(&pool->list_mutex);
    pthread_cond_broadcast(&pool->list_cond);
}

static
pnl_job_t *wait_and_pop_job(pnl_thread_pool_t *pool) {
    pnl_list_t *list_entry;

    pthread_mutex_lock(&pool->list_mutex);
    while (pnl_list_is_empty(&pool->job_list) && !is_job_list_blocked(pool)) {
        pthread_cond_wait(&pool->list_cond, &pool->list_mutex);
    }

    list_entry = pnl_list_first(&pool->job_list);

    if(list_entry){
        pnl_list_remove(list_entry);
    }

    pthread_mutex_unlock(&pool->list_mutex);

    return list_entry ? PNL_LIST_ENTRY(list_entry, pnl_job_t, node) : NULL;
}

static inline
void delete_all_jobs(pnl_thread_pool_t *pool) {
    pnl_list_t *list_entry;
    pthread_mutex_lock(&pool->list_mutex);
    while (!pnl_list_is_empty(&pool->job_list)) {
        list_entry = pnl_list_first(&pool->job_list);
        free(PNL_LIST_ENTRY(list_entry, pnl_job_t, node));
    }
    pthread_mutex_unlock(&pool->list_mutex);
}

static inline
pnl_thread_pool_t *allocate_pool(size_t number_of_threads) {
    pnl_thread_pool_t *pool = malloc(sizeof(pnl_thread_pool_t));
    if (pool) {
        pool->threads = malloc(sizeof(pnl_thread_t) * number_of_threads);
        if (!pool->threads) {
            free(pool);
            pool = NULL;
        }
    }
    return pool;
}

static inline
void deallocate_pool(pnl_thread_pool_t *pool) {
    free(pool->threads);
    free(pool);
}

static inline
int initialize_new_worker(pnl_thread_t *worker, pnl_thread_pool_t* pool) {
    return pthread_create(&worker->real_thread, NULL, main_worker_routine, pool);
}


static inline
void join_thread(pnl_thread_t *worker) {
    pthread_join(worker->real_thread, NULL);
}

static inline
void join_threads(pnl_thread_pool_t *pool, size_t number_of_threads) {
    for (size_t i = 0; i < number_of_threads; ++i) {
        join_thread(&pool->threads[i]);
    }
}


static int initialize_pool(pnl_thread_pool_t *pool, size_t number_of_threads) {

    int rc = PNL_OK;
    pool->number_of_threads = number_of_threads;
    pnl_list_init(&pool->job_list);
    pool->list_mutex = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    pool->list_cond = (pthread_cond_t) PTHREAD_COND_INITIALIZER;
    pool->is_list_blocked_lock = (pthread_rwlock_t) PTHREAD_RWLOCK_INITIALIZER;
    pool->is_list_blocked = 0;

    for (size_t i = 0; i < number_of_threads; ++i) {
        if (initialize_new_worker(&pool->threads[i],pool) != 0) {
            block_job_list(pool);
            join_threads(pool, i - 1); /*-1 because the latest initialization failed*/
            rc = PNL_ERR;
            break;
        }
    }

    return rc;
}


static void *main_worker_routine(void *thread_data) {
    pnl_thread_pool_t *pool = thread_data;
    pnl_job_t *job;
    while (1) {
        job = wait_and_pop_job(pool);
        if (job) {
            job->routine(job->data);
            free(job);
        } else {
            break;
        }
    }
    return NULL;
}


pnl_thread_pool_t *pnl_thread_pool_create(size_t number_of_threads) {

    pnl_thread_pool_t *pool;

    if (number_of_threads == 0) {
        number_of_threads = (size_t) get_nprocs() * 2;
    }

    pool = allocate_pool(number_of_threads);
    if (pool) {
        if (initialize_pool(pool, number_of_threads) == PNL_ERR) {
            deallocate_pool(pool);
        }
    }

    return pool;
}


int pnl_thread_pool_schedule_job(pnl_thread_pool_t *pool, job_routine routine, void *data) {
    int rc = PNL_OK;

    if (is_job_list_blocked(pool)) {
        return PNL_ERR;
    }

    pnl_job_t *job = malloc(sizeof(pnl_job_t));
    if (job) {
        job->data = data;
        job->routine = routine;
        push_job_and_notify(pool, job);
    } else {
        rc = PNL_ERR;
    }

    return rc;
}


void pnl_thread_pool_finish(pnl_thread_pool_t *pool) {
    if(!is_job_list_blocked(pool)){
        block_job_list(pool);
        join_threads(pool, pool->number_of_threads);
    }

}


void pnl_thread_pool_free(pnl_thread_pool_t *pool) {
    if(!is_job_list_blocked(pool)){ /*checks whether pnl_thread_pool_finish was called previously*/
        block_job_list(pool);
        delete_all_jobs(pool);
        join_threads(pool, pool->number_of_threads);
    }
    pthread_mutex_destroy(&pool->list_mutex);
    pthread_cond_destroy(&pool->list_cond);
    pthread_rwlock_destroy(&pool->is_list_blocked_lock);
    deallocate_pool(pool);
}



