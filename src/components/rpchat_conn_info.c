/** @file rpchat_conn_info.c
 *
 * @brief Implements definitions for rpchat_conn_info object used to track
 * connection-specific properties
 *
 * @par
 * COPYRIGHT NOTICE: None
 */

#include "components/rpchat_conn_info.h"

#include "rpchat_process_event.h"

int
rpchat_conn_info_initialize(rpchat_conn_info_t *p_new_conn_info, int h_new_fd)
{
    int res = RPLIB_UNSUCCESS;

    // set fields
    p_new_conn_info->h_fd        = h_new_fd;
    p_new_conn_info->conn_status = RPCHAT_CONN_PRE_REGISTER;
    pthread_mutex_init(&p_new_conn_info->mutex_conn, NULL);
    p_new_conn_info->username.len = 0;
    p_new_conn_info->stat_msg.len = 0;
    atomic_store(&p_new_conn_info->pending_jobs, 0);
    p_new_conn_info->last_active=time(0);

    res=RPLIB_SUCCESS;

    return res;
}

int
rpchat_conn_info_enqueue_task(rpchat_conn_info_t *p_conn_info,
                              rplib_tpool_t      *p_tpool,
                              void (*p_function)(void *p_arg),
                              void *p_arg)
{
    int res = RPLIB_UNSUCCESS;
    res     = rplib_tpool_enqueue_task(p_tpool, p_function, p_arg);
    // update counter
    if (RPLIB_SUCCESS == res)
    {
        atomic_fetch_add(&p_conn_info->pending_jobs, 1);
    }

    return res;
}
