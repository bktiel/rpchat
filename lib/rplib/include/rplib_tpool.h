/** @file rplib_hashmap.h
*
* @brief Basic threadpool implementation
*
* @par
* COPYRIGHT NOTICE: None
 */

#include "pthread.h"
#include "rplib_ll_queue.h"

typedef struct {
    rplib_ll_queue_t* p_tpool_tasks;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
} rplib_tpool_t;

/*** end of file ***/
