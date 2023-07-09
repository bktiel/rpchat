/** @file rpchat_basic_chat.c
 *
 * @brief Implements definitions specific to provided basic chat protocol
 *
 * @par
 * COPYRIGHT NOTICE: None
 */

#include "rpchat_basic_chat.h"



int
rpchat_begin_chat_server(unsigned int port_num,
                         unsigned int max_connections)
{
    int                  res             = RPLIB_UNSUCCESS; // assume failure
    int                  h_fd_server     = RPLIB_ERROR;     // server socket fd
    int                  h_fd_epoll      = RPLIB_ERROR;     // epoll instance fd
    int                  h_fd_signal     = RPLIB_ERROR;
    int                  loop_res        = RPLIB_SUCCESS;   // default success
    rplib_tpool_t       *p_tpool         = NULL;            // threadpool
    rpchat_conn_queue_t *p_conn_queue    = NULL; // queue for connections
    struct epoll_event  *p_ret_event_buf = NULL; // buffer for events

    // create tcp server socket and epoll instance
    res = rpchat_begin_networking(
        port_num, &h_fd_server, &h_fd_epoll, &h_fd_signal);
    if (0 > h_fd_epoll)
    {
        goto leave;
    }

    // create threadpool
    p_tpool = rplib_tpool_create(RPCHAT_NUM_THREADS);
    if (!p_tpool)
    {
        goto cleanup;
    }

    // create queue for connections
    p_conn_queue = rpchat_conn_queue_create(h_fd_epoll);
    if (!p_conn_queue)
    {
        goto cleanup;
    }

    // start threadpool
    rplib_tpool_start(p_tpool);

    // begin awaiting events
    for (;;)
    {
        // allocate returned events buffer
        p_ret_event_buf = calloc(max_connections, sizeof(struct epoll_event));
        if (NULL == p_ret_event_buf)
        {
            perror("calloc");
            res = RPLIB_ERROR;
            goto cleanup;
        }

        // wait for activity reported by epoll
        loop_res = rpchat_monitor_connections(
            h_fd_epoll, p_ret_event_buf, max_connections);

        if (RPLIB_ERROR == loop_res)
        {
            RPLIB_DEBUG_PRINTF("Error: %s \n", "Monitor Connections error");
            res = RPLIB_UNSUCCESS;
            break;
        }
        // handle incoming connections
        loop_res = rpchat_handle_events(p_ret_event_buf,
                                        h_fd_server,
                                        h_fd_epoll,
                                        h_fd_signal,
                                        p_tpool,
                                        loop_res,
                                        p_conn_queue);
        // if handle_events returns 1, planned exit
        if (RPLIB_UNSUCCESS == loop_res)
        {
            res = RPLIB_SUCCESS;
            break;
        }

        // cleanup per loop
        free(p_ret_event_buf);
        p_ret_event_buf = NULL;
    }
    // notify
    printf("\nNotice: %s\n", "Shutting down..");
cleanup:
    // clean tpool, allow jobs to finish
    if (NULL != p_tpool)
    {
        rplib_tpool_destroy(p_tpool, false);
    }
    // clean up conn_queue
    if (NULL != p_conn_queue)
    {
        rpchat_conn_queue_destroy(p_conn_queue);
    }
    // clean up epoll (and watched fds)
    if (0 < h_fd_epoll)
    {
        rpchat_stop_networking(h_fd_epoll, h_fd_server, h_fd_signal);
    }
    free(p_ret_event_buf);
    p_ret_event_buf = NULL;
leave:
    return res;
}

int
rpchat_handle_events(struct epoll_event  *p_ret_event_buf,
                     int                  h_fd_server,
                     int                  h_fd_epoll,
                     int                  h_fd_signal,
                     rplib_tpool_t       *p_tpool,
                     size_t               sz_ret_event_buf,
                     rpchat_conn_queue_t *p_conn_queue)
{
    int                       res             = RPLIB_UNSUCCESS;
    unsigned int              event_index     = 0;    // index for event loop
    rpchat_conn_info_t       *p_conn_info     = NULL;
    rpchat_args_proc_event_t *p_new_proc_args = NULL; // args for each event

    // iterate over returned events
    for (event_index = 0; event_index < sz_ret_event_buf; event_index++)
    {
        // process signal
        if (h_fd_signal == p_ret_event_buf[event_index].data.fd)
        {
            res = rpchat_handle_signal(
                h_fd_epoll, h_fd_signal, p_tpool, p_conn_queue);
            // success is the only return that allows continue
            if (RPLIB_SUCCESS == res)
            {
                continue;
            }
            goto leave;
        }
        // process new connection
        if (h_fd_server == p_ret_event_buf[event_index].data.fd)
        {
            res = rpchat_handle_new_connection(
                h_fd_server, h_fd_epoll, p_conn_queue);
            if (RPLIB_SUCCESS != res)
            {
                goto leave;
            }
            else
            {
                continue;
            }
        }
        // handle new event on existing connection
        // allocate (task args will be freed by callee)
        p_new_proc_args = NULL;
        p_new_proc_args = malloc(sizeof(rpchat_args_proc_event_t));
        if (!p_new_proc_args)
        {
            goto cleanup;
        }
        p_conn_info
            = (rpchat_conn_info_t *)p_ret_event_buf[event_index].data.ptr;
        // set fields for arg object
        p_new_proc_args->p_msg_buf    = NULL;
        p_new_proc_args->sz_msg_buf   = 0;
        p_new_proc_args->p_tpool      = p_tpool;
        p_new_proc_args->p_conn_queue = p_conn_queue;
        p_new_proc_args->p_conn_info  = p_conn_info;
        p_new_proc_args->args_type    = RPCHAT_PROC_EVENT_INBOUND;
        // copy over event
        memcpy(&p_new_proc_args->epoll_event,
               &p_ret_event_buf[event_index],
               sizeof(struct epoll_event));

        // stop listening for incoming
        rpchat_toggle_descriptor(
            h_fd_epoll, p_conn_info->h_fd, p_conn_info, false);

        // send to threadpool
        res = rpchat_conn_info_enqueue_task(
            (rpchat_conn_info_t *)p_ret_event_buf[event_index].data.ptr,
            p_tpool,
            rpchat_task_conn_proc_event,
            p_new_proc_args);
    }
    goto leave;
cleanup:
    free(p_new_proc_args);
    p_new_proc_args = NULL;
leave:
    return res;
}

int
rpchat_handle_new_connection(unsigned int         h_fd_server,
                             unsigned int         h_fd_epoll,
                             rpchat_conn_queue_t *p_conn_queue)
{
    int                    h_new_fd = RPLIB_ERROR;
    int                    res      = RPLIB_ERROR;
    rplib_ll_queue_node_t *p_new_node;
    rpchat_conn_info_t     new_conn_info;
    struct epoll_event     new_event;

    // accept connection
    h_new_fd = rpchat_accept_new_connection(h_fd_server);
    if (0 > h_new_fd)
    {
        goto leave;
    }

    // set fields
    rpchat_conn_info_initialize(&new_conn_info, h_new_fd);

    // enqueue new conn info
    pthread_mutex_lock(&p_conn_queue->mutex_conn_ll);
    p_new_node = rplib_ll_queue_enqueue(
        p_conn_queue->p_conn_ll, &new_conn_info, sizeof(rpchat_conn_info_t));
    if (!p_new_node)
    {
        goto leave;
    }

    // assign
    new_event.events   = (EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET);
    new_event.data.ptr = p_new_node->p_data;
    res = epoll_ctl(h_fd_epoll, EPOLL_CTL_ADD, h_new_fd, &new_event);

leave:
    pthread_mutex_unlock(&p_conn_queue->mutex_conn_ll);
    return res;
}

/**
 * Helper function to check connections for timeout.
 * If a connection has timed out, tell main event loop to gracefully d/c and
 * destroy it.
 * @param p_conn_queue Pointer to connection queue
 * @param p_tpool Pointer to threadpool
 * @return RPLIB_SUCCESS on no issues, otherwise RPLIB_UNSUCCESS
 */
static int
rpchat_audit_connections(rpchat_conn_queue_t *p_conn_queue,
                         rplib_tpool_t       *p_tpool)
{
    rplib_ll_queue_node_t    *p_current_node;
    rpchat_conn_info_t       *p_current_info;
    rpchat_args_proc_event_t *p_exit_args; // arguments to close a conn
    int                       res = RPLIB_UNSUCCESS;

    // acquire lock on connection queue for safety
    pthread_mutex_lock(&p_conn_queue->mutex_conn_ll);
    // if queue empty, wave off
    if (1 > p_conn_queue->p_conn_ll->size)
    {
        res = RPLIB_SUCCESS;
        goto leave;
    }
    p_current_node = p_conn_queue->p_conn_ll->p_front;
    while (NULL != p_current_node)
    {
        p_current_info = (rpchat_conn_info_t *)p_current_node->p_data;
        // check last active against timeout
        if (time(0) - p_current_info->last_active > RPCHAT_CONNECTION_TIMEOUT)
        {
            // allocate
            p_exit_args = malloc(sizeof(rpchat_args_proc_event_t));

            if (NULL == p_exit_args)
            {
                goto leave;
            }

            // set up args
            p_exit_args->p_conn_queue = p_conn_queue;
            p_exit_args->sz_msg_buf   = 0;
            p_exit_args->args_type    = RPCHAT_PROC_EVENT_INACTIVE;
            p_exit_args->p_tpool      = p_tpool;
            p_exit_args->p_msg_buf    = NULL;
            p_exit_args->p_conn_info  = p_current_info;

            res = rpchat_conn_info_enqueue_task(p_current_info,
                                                p_tpool,
                                                rpchat_task_conn_proc_event,
                                                p_exit_args);
            if (RPLIB_SUCCESS != res)
            {
                goto leave;
            }
        }
        p_current_node = p_current_node->p_next_node;
    }
    res = RPLIB_SUCCESS;
leave:
    pthread_mutex_unlock(&p_conn_queue->mutex_conn_ll);
    return res;
}

int
rpchat_handle_signal(int                  h_fd_epoll,
                     int                  h_fd_signal,
                     rplib_tpool_t       *p_tpool,
                     rpchat_conn_queue_t *p_conn_queue)
{
    int res    = RPLIB_ERROR;
    int signum = rpchat_get_signal(h_fd_signal);

    // get signumber
    // on SIGINT, stop listening and tell caller to close
    switch (signum)
    {
        case SIGINT:
            // stop listening
            epoll_ctl(h_fd_epoll, EPOLL_CTL_DEL, h_fd_signal, NULL);
            res = RPLIB_UNSUCCESS;
            goto leave;
        case SIGALRM:
            res = rpchat_audit_connections(p_conn_queue, p_tpool);
        default:
            goto leave;
    }

leave:
    return res;
}
