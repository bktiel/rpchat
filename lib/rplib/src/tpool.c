/**
 * @file p_tpool.c
 * @brief Thread pool implementation
 *
 */
#include "tpool.h"

#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "prior_ll_lib.h"
#include "rp_common.h"

/**
 * Helper function
 * Compares two pieces of data as integer and returns result
 * @param data_1 pointer to first operand
 * @param data_2 pointer to second operand
 * @return 0 if equal, 1 if data_1 is larger, 2 if data_2 is larger, -1 on error
 */
static int
compare_int(void *data_1, void *data_2)
{
    int op1 = 0;
    int op2 = 0;
    // NULL edge case
    if (data_2 == NULL || data_1 == NULL)
    {
        if (data_1 == data_2)
        {
            return 0;
        }
        else
        {
            return -1;
        }
    }

    // cast and compare
    op1 = *(int *)data_1;
    op2 = *(int *)data_2;
    if (op1 == op2)
    {
        return 0;
    }
    if (op1 > op2)
    {
        return 1;
    }
    if (op1 < op2)
    {
        return 2;
    }
    return -1;
}

/**
 * Check if there are jobs waiting in a given thread pool\n
 * WARNING: Non-atomic!
 * @param p_tpool Pointer to a threadpool
 * @return RPLIB_SUCCESS if empty or error, 1 if jobs present
 */
static bool
tpool_check_has_jobs(tpool_t *p_tpool)
{
    if (!p_tpool || !p_tpool->p_job_queue)
    {
        return RPLIB_SUCCESS;
    }
    // return results of empty
    return (0 < p_tpool->p_job_queue->size);
}

/**
 * Frees a given p_job from memory
 * @param p_job Pointer to p_job object
 * @return RPLIB_SUCCESS on success, 1 on error
 */
static bool
tpool_destroy_job(tpool_job_t *p_job)
{
    if (!p_job)
    {
        return RPLIB_UNSUCCESS;
    }
    free(p_job);
    p_job = NULL;
    return RPLIB_SUCCESS;
}

static void *
tpool_thread_internal(tpool_t *p_tpool)
{
    tpool_job_t *p_job = NULL;
    for (;;)
    {
        // check for jobs
        // acquire lock on pqueue object for jobs
        // if already locked by another obj, this blocks execution until free
        pthread_mutex_lock(&p_tpool->tpool_pqueue_mutex);

        // update status
        atomic_fetch_add(&p_tpool->waiting, 1);

        // if acquired lock, wait for 1) pqueue to have jobs and 2) acquire
        // condition on p_tpool object
        // use while loop to handle spurious wakeups (tldr race condition)
        while (!tpool_check_has_jobs(p_tpool)
               && (0 != atomic_load(&(p_tpool->running))))
        {
            pthread_cond_wait(&p_tpool->tpool_thread_cond,
                              &p_tpool->tpool_pqueue_mutex);
            /* pthread_cond_wait automatically unlocks mutex and blocks until
             * condition is met. This allows other p_threads to do this same
             * check and block on condition note that the while loop here lets
             * the thread essentially double-check whether or not another thread
             * has satisfied the wakeup by checking again if there are any items
             * to process
             */
        }

        // if threadpool says to stop and there are no jobs, quit
        if (0 == atomic_load(&p_tpool->running)
            && !tpool_check_has_jobs(p_tpool))
        {
            atomic_fetch_sub(&p_tpool->waiting, 1);
            break;
        }

        // update status
        atomic_fetch_sub(&p_tpool->waiting, 1);
        atomic_fetch_add(&p_tpool->seeking, 1);
        // if signaled and lock acquired, pop p_job and execute
        p_job = prior_ll_dequeue(p_tpool->p_job_queue);
        // update status
        atomic_fetch_sub(&p_tpool->seeking, 1);
        atomic_fetch_add(&p_tpool->working, 1);
        // unlock
        pthread_mutex_unlock(&p_tpool->tpool_pqueue_mutex);
        // execute
        if (p_job != NULL)
        {
            p_job->job(p_tpool, p_job->p_args);
            // free
            tpool_destroy_job(p_job);
        }

        /* Reacquire lock, check if:
         * 1) Tpool is still running (eg not stopped)
         * 2) There are no working p_threads
         * 3) There are no Jobs in the threadpool queue
         * If all these conditions met, signal condition to make sure all
         * p_threads are actively awaiting new jobs (and restart this procedure)
         */
        atomic_fetch_sub(&p_tpool->working, 1);
        pthread_mutex_lock(&p_tpool->tpool_pqueue_mutex);
        if ((0 == atomic_load(&p_tpool->working))
            && (1 == atomic_load(&p_tpool->running))
            && !tpool_check_has_jobs(p_tpool))
        {
            pthread_cond_signal(&p_tpool->tpool_thread_cond);
        }
        // if threadpool says to clear jobs, make it so
        if (1 == atomic_load(&p_tpool->clear))
        {
            prior_ll_destroy(&p_tpool->p_job_queue);
            // note that if the job queue is deleted
            // check_has_jobs will return false so it gets us where we need to
            // go
        }
        pthread_mutex_unlock(&p_tpool->tpool_pqueue_mutex);
    }

    // signal for tpool_wait
    pthread_cond_broadcast(&p_tpool->tpool_thread_cond);
    // unlock previous lock on work mutex
    pthread_mutex_unlock(&p_tpool->tpool_pqueue_mutex);
    // destroy thread
    atomic_fetch_sub_explicit(&p_tpool->alive, 1, memory_order_release);

    return NULL;
}

/**
 * Create ThreadPool object with specified number of p_threads.\n
 * Creates heap p_buf to track created p_threads, tear down with
 * tpool_shutdown or tpool_destroy
 * @param num_threads Number of p_threads to use in threadpool
 * @return Created threadpool, NULL on failure
 */

tpool_t *
tpool_new(size_t num_threads)
{
    tpool_t  temp_pool  = THREADPOOL_STATIC_INITIALIZER;
    tpool_t *p_new_pool = NULL;

    // if 0, set to 4 to start
    if (num_threads < 1)
    {
        num_threads = 4;
    }

    // init fields
    temp_pool.num_threads = num_threads;
    // queue
    temp_pool.p_job_queue = prior_ll_create_queue();
    if (!temp_pool.p_job_queue)
    {
        p_new_pool=NULL;
        goto leave;
    }

    // Make allocations (p_tpool object and thread array)
    p_new_pool = malloc(sizeof(temp_pool));
    if (!p_new_pool)
    {
        p_new_pool=NULL;
        goto leave;
    }

    // allocate p_buf for thread array
    temp_pool.p_threads = malloc(sizeof(pthread_t) * num_threads);
    if (!temp_pool.p_threads)
    {
        temp_pool.p_threads=NULL;
        goto cleanup;
    }

    // transfer fields from stack obj to heap obj
    memcpy(p_new_pool, &temp_pool, sizeof(temp_pool));

    // create p_threads
    for (size_t i = 0; i < num_threads; i++)
    {
        /* creates a new thread handle for each index of thread p_buf, assigns
         * internal task as work with an argument of the new threadpool (IOT
         * refer back)
         */
        if (pthread_create(&p_new_pool->p_threads[i],
                           NULL,
                           tpool_thread_internal,
                           p_new_pool)
            || pthread_detach(p_new_pool->p_threads[i]))
        {
            // error on failure to create/detach
            return NULL;
        }
        // update alive
        atomic_fetch_add(&p_new_pool->alive, 1);
    }
    goto leave;
cleanup:
    free(p_new_pool);
    free(temp_pool.p_threads);
    p_new_pool=NULL;
    temp_pool.p_threads=NULL;
leave:
    return p_new_pool;
}

void
tpool_destroy(tpool_t *p_tpool)
{
    assert(p_tpool);
    // free thread array
    if (p_tpool->p_threads)
    {
        free(p_tpool->p_threads);
        p_tpool->p_threads = NULL;
    }

    free(p_tpool);
    p_tpool = NULL;
}

int
tpool_shutdown(tpool_t *p_tpool, bool finish)
{
    tpool_job_t *deljob = NULL;
    if (!p_tpool)
    {
        return RPLIB_UNSUCCESS;
    }
    // if desire graceful finish and have p_threads to work with..
    if (finish && (0 < atomic_load(&p_tpool->alive)))
    {
        // wait for jobs to disappear organically
        for (;;)
        {
            bool has_jobs = false;
            pthread_mutex_lock(&p_tpool->tpool_pqueue_mutex);
            has_jobs = tpool_check_has_jobs(p_tpool);
            pthread_mutex_unlock(&p_tpool->tpool_pqueue_mutex);
            // get out of loop
            if (!has_jobs)
                break;
        }
    }
    else
    {
        // lock work to destroy all work
        tpool_clear_jobs(p_tpool);
    }
    // set p_tpool to dead and alert all p_threads
    atomic_store(&p_tpool->running, 0);
    pthread_cond_signal(&p_tpool->tpool_thread_cond);

    // block until p_threads complete cleanup
    tpool_wait(p_tpool);

    // free data structures
    prior_ll_destroy(&p_tpool->p_job_queue);
    pthread_mutex_lock(&p_tpool->tpool_pqueue_mutex);
    pthread_mutex_lock(&p_tpool->tpool_thread_mutex);
    pthread_cond_destroy(&p_tpool->tpool_thread_cond);
    // destroy mutexes
    pthread_mutex_unlock(&p_tpool->tpool_pqueue_mutex);
    pthread_mutex_unlock(&p_tpool->tpool_thread_mutex);
    pthread_mutex_destroy(&p_tpool->tpool_pqueue_mutex);
    pthread_mutex_destroy(&p_tpool->tpool_thread_mutex);

    // destroy object
    tpool_destroy(p_tpool);
    p_tpool = NULL;

    return RPLIB_SUCCESS;
}

int
tpool_push(tpool_t *p_tpool, job_func job, void *p_args, int priority)
{
    tpool_job_t *p_new_job = NULL;
    if (!p_tpool)
    {
        return RPLIB_UNSUCCESS;
    }
    // allocate
    p_new_job = malloc(sizeof(tpool_job_t));
    if (!p_new_job)
    {
        return RPLIB_UNSUCCESS;
    }
    // fields
    p_new_job->job      = job;
    p_new_job->p_args   = p_args;
    p_new_job->priority = priority;

    // get work lock and enqueue
    pthread_mutex_lock(&p_tpool->tpool_pqueue_mutex);
    prior_ll_enqueue(
        p_tpool->p_job_queue, p_new_job, compare_int, &p_new_job->priority);
    // unlock and signal addition to all p_threads
    pthread_mutex_unlock(&p_tpool->tpool_pqueue_mutex);
    pthread_cond_signal(&p_tpool->tpool_thread_cond);

    return RPLIB_SUCCESS;
}

int
tpool_spin(tpool_t *p_tpool)
{
}

/**
 * Clear jobs from rplib queue
 * @param p_tpool
 * @return
 */
int
tpool_clear_jobs(tpool_t *p_tpool)
{
    tpool_job_t *deljob = NULL;
    pthread_mutex_lock(&p_tpool->tpool_pqueue_mutex);
    deljob = prior_ll_dequeue(p_tpool->p_job_queue);
    while (NULL != deljob)
    {
        tpool_destroy_job(deljob);
        deljob = prior_ll_dequeue(p_tpool->p_job_queue);
    }
    pthread_mutex_unlock(&p_tpool->tpool_pqueue_mutex);
    return RPLIB_SUCCESS;
}

size_t
atomic_tpool_has_jobs(tpool_t *p_tpool)
{
    size_t size = 0;
    pthread_mutex_lock(&p_tpool->tpool_pqueue_mutex);
    size = p_tpool->p_job_queue->size;
    pthread_mutex_unlock(&p_tpool->tpool_pqueue_mutex);
    return (size > 0);
}

void
tpool_wait(tpool_t *p_tpool)
{
    {
        if (!p_tpool)
        {
            return;
        }

        // acquire work lock
        // pthread_mutex_lock(&p_tpool->tpool_pqueue_mutex);
        // acquire field lock
        pthread_mutex_lock(&p_tpool->tpool_thread_mutex);
        // loop
        for (;;)
        {
            /*
             * Run if p_tpool is alive and has p_threads still working
             * Or p_tpool NOT alive but has multiple p_threads
             */
            int cond1 = atomic_load(&p_tpool->running);
            int cond2 = THREADPOOL_STOPPED_THREADS_ALIVE(p_tpool);
            if (cond1 || cond2)
            {
                // this wait should end up triggered by the p_threads closing
                // out
                pthread_cond_wait(&p_tpool->tpool_thread_cond,
                                  &p_tpool->tpool_thread_mutex);
                // then loop back over
            }
            else
            {

                // if p_tpool successfully closed out (working=0,#p_threads=0),
                // break out of blocking
                break;
            }
        }
        pthread_mutex_unlock(&p_tpool->tpool_thread_mutex);
    }
}

void
tpool_join(tpool_t *p_tpool)
{
    if (!p_tpool)
    {
        return;
    }

    // acquire work lock
    // pthread_mutex_lock(&p_tpool->tpool_pqueue_mutex);
    // acquire field lock
    pthread_mutex_lock(&p_tpool->tpool_thread_mutex);
    // loop
    for (;;)
    {
        /*
         * Run if p_tpool is alive and has p_threads still working
         * Or p_tpool NOT alive but has multiple p_threads
         */
        int cond1 = THREADPOOL_RUNNING_THREADS_WORKING(p_tpool);
        int cond2 = THREADPOOL_STOPPED_THREADS_ALIVE(p_tpool);
        if (cond1 || cond2)
        {
            // this wait should end up triggered by the p_threads closing
            // out
            pthread_cond_wait(&p_tpool->tpool_thread_cond,
                              &p_tpool->tpool_thread_mutex);
            // then loop back over
        }
        else
        {
            // if p_tpool successfully closed out (working=0,#p_threads=0),
            // break out of blocking
            break;
        }
    }
    pthread_mutex_unlock(&p_tpool->tpool_thread_mutex);
}
