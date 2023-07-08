/** @file rplib_tpool.c
 *
 * @brief Basic threadpool implementation
 *
 * @par
 * COPYRIGHT NOTICE: None
 */

#include "rplib_tpool.h"

#include <assert.h>
#include <stdbool.h>

rplib_tpool_t *
rplib_tpool_create(size_t num_threads)
{
    rplib_tpool_t *p_tpool = NULL;
    // allocate
    p_tpool = malloc(sizeof(rplib_tpool_t));
    if (!p_tpool)
    {
        goto leave;
    }
    // init
    rplib_tpool_initialize(p_tpool, num_threads);
leave:
    return p_tpool;
}

int
rplib_tpool_initialize(rplib_tpool_t *p_tpool, size_t num_threads)
{
    int res = RPLIB_UNSUCCESS;
    // asserts
    assert(p_tpool);
    assert(num_threads > 0);
    // set fields
    p_tpool->num_threads       = num_threads;
    p_tpool->num_threads_busy  = 0;
    p_tpool->num_threads_alive = 0;
    p_tpool->p_queue_tasks     = rplib_ll_queue_create();
    p_tpool->p_thread_buf      = calloc(num_threads, sizeof(pthread_t));

    atomic_store(&p_tpool->b_terminate, false);

    // shutdown
    // initialize pthread specific objects
    if (0 != pthread_mutex_init(&(p_tpool->mutex_task_queue), NULL)
        || 0 != pthread_mutex_init(&(p_tpool->mutex_thrd_count), NULL)
        || 0 != pthread_cond_init(&(p_tpool->cond_task_queue), NULL)
        || 0 != pthread_cond_init(&(p_tpool->cond_threads_idle), NULL))
    {
        goto leave;
    }
    // successful init
    res = RPLIB_SUCCESS;
leave:
    return res;
}

int
rplib_tpool_destroy(rplib_tpool_t *p_tpool, bool b_shutdown_immediate)
{
    int    res          = RPLIB_UNSUCCESS;
    size_t thread_index = 0;
    // if not urgent, wait for tasks to complete
    if (!b_shutdown_immediate)
    {
        rplib_tpool_wait(p_tpool);
    }

    // set termination flag and tell all threads to wake up
    atomic_store(&(p_tpool->b_terminate), 1);
    pthread_cond_broadcast(&p_tpool->cond_task_queue);

    // join threads
    for (thread_index = 0; thread_index < p_tpool->num_threads;
         thread_index++)
    {
        pthread_join(p_tpool->p_thread_buf[thread_index], NULL);
    }

    // destroy mutexes
    pthread_mutex_destroy(&(p_tpool->mutex_task_queue));
    pthread_mutex_destroy(&(p_tpool->mutex_thrd_count));
    pthread_cond_destroy(&(p_tpool->cond_task_queue));
    pthread_cond_destroy(&(p_tpool->cond_threads_idle));
    // destroy tasks
    res = rplib_ll_queue_destroy(p_tpool->p_queue_tasks);
    // destroy threadbuf
    free(p_tpool->p_thread_buf);
    // destroy tpool
    free(p_tpool);
    p_tpool = NULL;
    return res;
}

int
rplib_tpool_enqueue_task(rplib_tpool_t *p_tpool,
                         void (*p_function)(void *p_arg),
                         void *p_arg)
{
    int                res = RPLIB_UNSUCCESS;
    rplib_tpool_task_t new_task; // new task to add to tpool

    // acquire lock
    pthread_mutex_lock(&(p_tpool->mutex_task_queue));
    // create task
    new_task.p_arg      = p_arg;
    new_task.p_function = p_function;
    // add to queue
    res = NULL
          != rplib_ll_queue_enqueue(
              p_tpool->p_queue_tasks, &new_task, sizeof(rplib_tpool_task_t));
    if (!res)
    {
        RPLIB_DEBUG_PRINTF("Error: %s\n", "TPOOL ENQUEUE");
        goto leave;
    }
    // signal that new job available
    pthread_cond_signal(&(p_tpool->cond_task_queue));
    // unlock
    pthread_mutex_unlock(&(p_tpool->mutex_task_queue));
    res = RPLIB_SUCCESS;
leave:
    return res;
}

void
rplib_tpool_thread_do(rplib_tpool_t *p_tpool)
{
    void (*p_function)(void *p_arg) = NULL; // generic ptr to func to run
    void *p_arg                     = NULL; // generic ptr to arg for func

    // if launched, we're alive
    pthread_mutex_lock(&p_tpool->mutex_thrd_count);
    p_tpool->num_threads_alive++;
    pthread_mutex_unlock(&p_tpool->mutex_thrd_count);

    for (;;)
    {

        // update working threadcount
        pthread_mutex_lock(&p_tpool->mutex_thrd_count);
        p_tpool->num_threads_busy++;
        pthread_mutex_unlock(&p_tpool->mutex_thrd_count);

        // acquire lock on queue (or attempt to lock until we can)
        pthread_mutex_lock(&(p_tpool->mutex_task_queue));
        // wait until signaled for new job
        while (p_tpool->p_queue_tasks->size == 0
               && !atomic_load(&(p_tpool->b_terminate)))
        {
            // not busy while waiting
            pthread_mutex_lock(&p_tpool->mutex_thrd_count);
            p_tpool->num_threads_busy--;
            pthread_mutex_unlock(&p_tpool->mutex_thrd_count);

            pthread_cond_wait(&(p_tpool->cond_task_queue),
                              &(p_tpool->mutex_task_queue));

            // busy again
            pthread_mutex_lock(&p_tpool->mutex_thrd_count);
            p_tpool->num_threads_busy++;
            pthread_mutex_unlock(&p_tpool->mutex_thrd_count);
        }

        // if shutdown, get out
        if (atomic_load(&(p_tpool->b_terminate)))
        {
            pthread_mutex_unlock(&(p_tpool->mutex_task_queue));
            break;
        }

        // new job has arrived
        p_function
            = ((rplib_tpool_task_t *)p_tpool->p_queue_tasks->p_front->p_data)
                  ->p_function;
        p_arg = ((rplib_tpool_task_t *)p_tpool->p_queue_tasks->p_front->p_data)
                    ->p_arg;
        // update queue
        rplib_ll_queue_dequeue(p_tpool->p_queue_tasks);

        // unlock mutex
        pthread_mutex_unlock(&p_tpool->mutex_task_queue);

        // run target function
        p_function(p_arg);

        // update tpool metrics
        pthread_mutex_lock(&p_tpool->mutex_thrd_count);
        p_tpool->num_threads_busy--;
        // if no threads working, trigger the idle condition for tpool_wait
        if (!p_tpool->num_threads_busy)
        {
            pthread_cond_signal(&p_tpool->cond_threads_idle);
        }
        pthread_mutex_unlock(&p_tpool->mutex_thrd_count);
    }

    // where were you when thread was kill
    pthread_mutex_lock(&p_tpool->mutex_thrd_count);
    p_tpool->num_threads_busy--;
    p_tpool->num_threads_alive--;
    pthread_mutex_unlock(&p_tpool->mutex_thrd_count);

    pthread_exit(NULL);
}

/**
 * Helper function to pass `rplib_tpool_thread_do` using func signature
 * required for `pthread_create`
 * @param p_tpool Pointer to `rplib_tpool_t`
 * @return NULL, always
 */
static void *
rplib_tpool_thread_start(void *p_tpool)
{
    rplib_tpool_thread_do((rplib_tpool_t *)p_tpool);
    return NULL;
}

int
rplib_tpool_start(rplib_tpool_t *p_tpool)
{
    int    res        = RPLIB_UNSUCCESS;
    size_t loop_index = 0;
    for (loop_index = 0; loop_index < p_tpool->num_threads; loop_index++)
    {
        res = pthread_create(&(p_tpool->p_thread_buf[loop_index]),
                             NULL,
                             rplib_tpool_thread_start,
                             (void *)p_tpool);
        if (0 != res)
        {
            RPLIB_DEBUG_PRINTF("Error: %s\n", "TPOOL THREAD CREATE");
            res = RPLIB_UNSUCCESS;
            goto leave;
        }
        RPLIB_DEBUG_PRINTF(
            "Notice: %s #%zu %s\n", "TPOOL THREAD", loop_index, "started.");
    }
    res = RPLIB_SUCCESS;
leave:
    return res;
}

int
rplib_tpool_wait(rplib_tpool_t *p_tpool)
{
    // keep waiting while:
    // a) job queue has items (non-zero)
    // b) there are threads working (non-zero)
    pthread_mutex_lock(&p_tpool->mutex_thrd_count);

    while (p_tpool->num_threads_busy || p_tpool->p_queue_tasks->size)
    {
        // await all threads idle condition
        pthread_cond_wait(&p_tpool->cond_threads_idle,
                          &p_tpool->mutex_thrd_count);
    }
    pthread_mutex_unlock(&p_tpool->mutex_thrd_count);

    // if we make it this far all threads are idle and there are no pending jobs
    return RPLIB_SUCCESS;
}
