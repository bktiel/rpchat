/**
 * @file p_tpool.h
 * @brief Implementation of a thread pool.
 *
 */
#ifndef CALC_TPOOL_H_
#define CALC_TPOOL_H_

#ifdef __STDC_NO_ATOMICS__
#error this implementation needs atomics
assert(false);
#endif
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>

// #include "calc_pqueue.h"

#include "prior_ll_lib.h"
// typedef existing library to type
typedef struct prior_ll_queue pqueue_t;

/**
 * @brief State of a thread at any given moment
 *
 */
typedef enum thread_state_t
{
    WORKING, /* running the job func */
    WAITING, /* waiting to be woken up */
    GETJOB,  /* attempting to pull a job off the queue */
} thread_state_t;

/**
 * @brief Thread pool implementation. Holds jobs in a priority queue and reports
 * num p_threads alive and in what states.
 *
 */
typedef struct tpool_t
{
    pqueue_t      *p_job_queue; /* priority queue for jobs */
    size_t         num_threads; /* number of p_threads spawned */
    pthread_t     *p_threads;   /* array of p_threads */
    atomic_bool    running;     /* whether pool is running */
    atomic_int     working;     /* # of p_threads working */
    atomic_int     waiting;     /* # of p_threads waiting for a job */
    atomic_int     seeking;     /* # of p_threads trying to get jobs */
    atomic_int     alive;       /* # of p_threads alive */
    atomic_int     clear;       /* Whether to clear entries */
    pthread_cond_t tpool_thread_cond; /* cond for p_threads to wait on if no job
                                         available */
    pthread_mutex_t tpool_thread_mutex; /* mutex to lock this struct if
                                           nonatomic members accessed */
    atomic_int      cond_check;         /* condition to test spurious wakeup */
    pthread_mutex_t tpool_pqueue_mutex; /* mutex specifically for pqueue */
} tpool_t;

#define THREADPOOL_STATIC_INITIALIZER                                       \
    {                                                                       \
        .p_job_queue = NULL, .num_threads = 0, .p_threads = NULL,           \
        .running = 1, .working = 0, .waiting = 0, .seeking = 0, .alive = 0, \
        .clear = 0,\
        .tpool_thread_cond  = PTHREAD_COND_INITIALIZER,                     \
        .tpool_thread_mutex = PTHREAD_MUTEX_INITIALIZER, .cond_check = 0,   \
        .tpool_pqueue_mutex = PTHREAD_MUTEX_INITIALIZER                     \
    }

#define THREADPOOL_RUNNING_THREADS_WORKING(tp) \
    ((0 != atomic_load(&((tp)->working))) && (true == atomic_load(&((tp)->running))))

#define THREADPOOL_STOPPED_THREADS_ALIVE(tp) \
    ((false == atomic_load(&((tp)->running))) && (0 != atomic_load(&((tp)->alive))))

/**
 * @brief Function prototype that thread pool runs.
 *
 */
typedef void (*job_func)(tpool_t *p_tpool, void *p_args);

/**
 * @brief A job on the queue.
 *
 */
typedef struct tpool_job_t
{
    job_func job;
    void    *p_args;
    int      priority;
} tpool_job_t;

/**
 * @brief Create a new thread pool
 *
 * @param num_threads
 * @return tpool_t*
 */
tpool_t *tpool_new(size_t num_threads);

/**
 * Blocks execution until threadpool running is set to false externally
 * @param p_tpool
 */
void tpool_wait(tpool_t *p_tpool);


/**
 * Blocks execution until all threads have finished working
 * @param p_tpool
 */
void tpool_join(tpool_t *p_tpool);

/**
 * @brief Free resources from a thread pool. Does not set to NULL.
 *
 * @param p_tpool
 */
void tpool_destroy(tpool_t *p_tpool);

/**
 * @brief Gracefully shutdown thread pool.
 *
 * @param p_tpool
 * @param finish Complete remaining jobs
 * @return int
 */
int tpool_shutdown(tpool_t *p_tpool, bool finish);

/**
 * @brief Add a job to the queue
 *
 * @param p_tpool thread pool instance
 * @param job Function to run
 * @param p_args Args to the function, will not be freed
 * @param priority Priority for the queue
 * @return int
 */
int tpool_push(tpool_t *p_tpool, job_func job, void *p_args, int priority);

/**
 * @brief Wake up a thread.
 *
 * @param p_tpool
 * @return int
 */
int tpool_spin(tpool_t *p_tpool);

/**
 * Delete all jobs in threadpool
 * @param p_tpool
 * @return
 */
int tpool_clear_jobs(tpool_t *p_tpool);

/**
 * Does threadpool have jobs
 * @param p_tpool
 * @return
 */
size_t atomic_tpool_has_jobs(tpool_t *p_tpool);

#endif // CALC_TPOOL_H_
