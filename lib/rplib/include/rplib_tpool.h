/** @file rplib_tpool.h
 *
 * @brief Basic threadpool implementation
 *
 * @par
 * COPYRIGHT NOTICE: None
 */

#ifndef RPLIB_TPOOL_H
#define RPLIB_TPOOL_H

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>

#include "rplib_common.h"
#include "rplib_ll_queue.h"

typedef struct
{
    rplib_ll_queue_t *p_queue_tasks;     // job queue
    pthread_t        *p_thread_buf;      // storage for threads
    size_t            num_threads;       // number of threads to use
    volatile size_t   num_threads_busy;  // number of active threads
    pthread_mutex_t   mutex_task_queue;  // lock for job queue
    pthread_mutex_t   mutex_thrd_count;  // lock for thread metrics
    pthread_cond_t    cond_task_queue;   // used to signal threads about jobs
    pthread_cond_t    cond_threads_idle; // used to signal all threads idle
    atomic_bool       b_shutdown;        // trigger for shutdown
} rplib_tpool_t;

typedef struct
{
    void (*p_function)(void *p_arg);
    void *p_arg;
} rplib_tpool_task_t;

/**
 * Create a threadpool object
 * @param num_threads Number of threads to use
 * @return Pointer to threadpool object; NULL on failure
 */
rplib_tpool_t *rplib_tpool_create(size_t num_threads);

/**
 * Initialize threadpool object
 * @param p_tpool Pointer to threadpool to initialize
 * @param num_threads Number of threads to use
 * @return RPLIB_SUCCESS on success; RP_UNSUCCESS otherwise
 */
int rplib_tpool_initialize(rplib_tpool_t *p_tpool, size_t num_threads);

/**
 * Destroy threadpool object
 * @param p_tpool Pointer to threadpool to destroy
 * @return RPLIB_SUCCESS on no issues; otherwise RPLIB_UNSUCCESS
 */
int rplib_tpool_destroy(rplib_tpool_t *p_tpool, bool b_shutdown_immediate);

/**
 * Enqueue a task in the threadpool
 * @param p_tpool Pointer to threadpool object
 * @param p_function Pointer to function to enqueue
 * @param p_arg Pointer to argument object to pass function
 * @return RPLIB_SUCCESS on success; otherwise RPLIB_UNSUCCESS
 */
int rplib_tpool_enqueue_task(rplib_tpool_t *p_tpool,
                             void (*p_function)(void *p_arg),
                             void *p_arg);

/**
 * Helper function that every thread executes persistently until threadpool
 * is destroyed
 * @param p_tpool Pointer to threadpool object
 * @return RPLIB_SUCCESS on no issues, RPLIB_UNSUCCESS otherwise
 */
void rplib_tpool_thread_do(rplib_tpool_t *p_tpool);

/**
 * Start threadpool with amount of threads specified in `rplib_tpool_initialize`
 * @param p_tpool Pointer to threadpool object
 * @return RPLIB_SUCCESS on no issues; otherwise RPLIB_UNSUCCESS
 */
int rplib_tpool_start(rplib_tpool_t *p_tpool);

/**
 * Thread join
 * @param p_tpool Pointer to threadpool object
 * @return RPLIB_SUCCESS on no issues; otherwise RPLIB_UNSUCCESS
 */
int rplib_tpool_wait(rplib_tpool_t *p_tpool);

#endif /* RPLIB_TPOOL_H */

/*** end of file ***/
