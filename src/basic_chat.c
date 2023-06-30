#include "basic_chat.h"

#include <assert.h>
#include <malloc.h>
#include <stdbool.h>
#include <string.h>

#include "endian.h"
#include "networking.h"
#include "rplib_common.h"
#include "rplib_tpool.h"

/** @file basic_chat.c
 *
 * @brief Implements definitions specific to provided basic chat protocol
 *
 * @par
 * COPYRIGHT NOTICE: None
 */

int
rpchat_begin_chat_server(unsigned int port_num, unsigned int max_connections)
{
    int                 res             = RPLIB_UNSUCCESS; // assume failure
    int                 h_fd_server     = RPLIB_ERROR;     // server socket fd
    int                 h_fd_epoll      = RPLIB_ERROR;     // epoll instance fd
    int                 loop_res        = RPLIB_SUCCESS;   // default success
    rplib_tpool_t      *p_tpool         = NULL;            // threadpool
    rplib_ll_queue_t   *p_conn_queue    = NULL; // queue for connections
    struct epoll_event *p_ret_event_buf = NULL; // buffer for events

    // create tcp server socket and epoll instance
    res = rpchat_begin_networking(port_num, &h_fd_server, &h_fd_epoll);
    if (0 > h_fd_epoll)
    {
        goto leave;
    }

    // create threadpool
    p_tpool = rplib_tpool_create(RPCHAT_NUM_THREADS);
    if (!p_tpool)
    {
        goto leave;
    }

    // create queue for connections
    p_conn_queue = rplib_ll_queue_create();
    if (!p_conn_queue)
    {
        goto leave;
    }

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
        if (RPLIB_UNSUCCESS == loop_res)
        {
            res = RPLIB_SUCCESS;
            break;
        }
        else if (RPLIB_ERROR == loop_res)
        {
            res = RPLIB_UNSUCCESS;
            break;
        }
        // handle incoming connections
        rpchat_handle_events(p_ret_event_buf,
                             h_fd_server,
                             h_fd_epoll,
                             p_tpool,
                             max_connections,
                             p_conn_queue);

        // cleanup per loop
        free(p_ret_event_buf);
        p_ret_event_buf = NULL;
    }

cleanup:
    rplib_ll_queue_destroy(p_conn_queue);
    free(p_ret_event_buf);
    p_ret_event_buf = NULL;

leave:
    return res;
}

/**
 * Turn an epoll descriptor on or off, making the epoll instance either ignore
 * or pay attention to inbound data
 * @param h_fd_epoll Epoll instance file descriptor
 * @param p_event Original event object associated with target connection
 * @param enabled If true, listen to events on this connection. If false, stop
 * listening.
 * @return RPLIB_SUCCESS on success, RPLIB_UNSUCCESS on problems
 */
static int
rpchat_toggle_descriptor(int                 h_fd_epoll,
                         struct epoll_event *p_event,
                         bool                enabled)
{
    int                res  = RPLIB_UNSUCCESS;
    int                h_fd = 0;
    struct epoll_event delta_event; // contains new defs
    // set defs
    h_fd = ((rpchat_conn_info_t *)p_event->data.ptr)->h_fd;
    if (enabled)
    {
        delta_event.events = (EPOLLIN | EPOLLERR | EPOLLHUP);
    }
    else
    {
        delta_event.events = 0;
    }
    delta_event.data.ptr = p_event->data.ptr;
    // mod descriptor
    epoll_ctl(h_fd_epoll, EPOLL_CTL_MOD, h_fd, &delta_event);
}

int
rpchat_handle_events(struct epoll_event *p_ret_event_buf,
                     int                 h_fd_server,
                     int                 h_fd_epoll,
                     rplib_tpool_t      *p_tpool,
                     unsigned int        max_connections,
                     rplib_ll_queue_t   *p_conn_queue)
{
    int                       res             = RPLIB_UNSUCCESS;
    int                       h_new_fd        = RPLIB_ERROR;
    unsigned int              event_index     = 0;    // index for event loop
    rpchat_args_proc_event_t *p_new_proc_args = NULL; // args for each event

    // iterate over returned events
    for (event_index = 0; event_index < max_connections; event_index++)
    {
        // process new connection
        if (h_fd_server == p_ret_event_buf[event_index].data.fd
            && RPLIB_SUCCESS
                   != rpchat_handle_new_connection(
                       h_fd_server, h_fd_epoll, p_conn_queue))
        {
            res = RPLIB_UNSUCCESS;
            goto leave;
        }
        // allocate (task args will be freed by callee)
        p_new_proc_args = malloc(sizeof(rpchat_args_proc_event_t));
        if (!p_new_proc_args)
        {
            goto cleanup;
        }
        // set fields for arg object
        p_new_proc_args->h_fd_epoll   = h_fd_epoll;
        p_new_proc_args->p_msg_buf    = NULL;
        p_new_proc_args->sz_msg_buf   = 0;
        p_new_proc_args->p_tpool      = p_tpool;
        p_new_proc_args->p_conn_queue = p_conn_queue;
        p_new_proc_args->args_type    = RPCHAT_MAX_INCOMING_PKT;
        // copy over event
        memcpy(&p_new_proc_args->epoll_event,
               &p_ret_event_buf[event_index],
               sizeof(struct epoll_event));

        // stop listening for incoming
        rpchat_toggle_descriptor(
            h_fd_epoll, &p_ret_event_buf[event_index], false);

        // send to threadpool
        rplib_tpool_enqueue_task(
            p_tpool, rpchat_task_conn_proc_event, p_new_proc_args);

        //        // process existing connection
        //        rpchat_handle_existing_connection(h_fd_server,
        //                                          h_fd_epoll,
        //                                          p_conn_queue,
        //                                          &p_ret_event_buf[event_index]);
    }
cleanup:
    free(p_new_proc_args);
    p_new_proc_args = NULL;
leave:
    return res;
}

int
rpchat_handle_new_connection(unsigned int      h_fd_server,
                             unsigned int      h_fd_epoll,
                             rplib_ll_queue_t *p_conn_queue)
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
    new_conn_info.h_fd        = h_new_fd;
    new_conn_info.conn_status = RPCHAT_CONN_PRE_REGISTER;
    pthread_mutex_init(&new_conn_info.mutex_conn, NULL);
    // enqueue new conn info
    p_new_node = rplib_ll_queue_enqueue(
        p_conn_queue, &new_conn_info, sizeof(rpchat_conn_info_t));
    if (!p_new_node)
    {
        goto leave;
    }
    // assign
    new_event.events   = (EPOLLIN | EPOLLERR | EPOLLHUP);
    new_event.data.ptr = p_new_node;
    res = epoll_ctl(h_fd_epoll, EPOLL_CTL_ADD, h_new_fd, &new_event);

leave:
    return res;
}

/**
 * Helper function for `rpchat_task_conn_proc_event`, when an event is
 * 'inbound,' receive the message and take an appropriate action.
 * @param p_task_args Pointer to parent `rpchat_args_proc_event_t` object
 * @return RPLIB_SUCCESS on success; otherwise RPLIB_UNSUCCESS, RPLIB_ERROR on
 * unrecoverable error..
 */
static int
rpchat_conn_proc_available_inbound(rpchat_args_proc_event_t *p_task_args)
{
    int                 res = RPLIB_UNSUCCESS;
    rpchat_conn_info_t *p_conn_info
        = (rpchat_conn_info_t *)p_task_args->epoll_event.data.ptr;
    rplib_tpool_t *p_tpool = p_task_args->p_tpool;

    // if there is no passed message, attempt to receive a message
    if (0 == p_task_args->sz_msg_buf)
    {
        // POLLIN = pending data on conn, attempt to process
        // POLLERR = has problem
        // POLLHUP = they gone
        if (p_task_args->epoll_event.events & EPOLLIN)
        {
            // receive message from client
            p_task_args->sz_msg_buf = rpchat_recvmsg(p_conn_info->h_fd,
                                                     RPCHAT_MAX_INCOMING_PKT,
                                                     p_task_args->p_msg_buf);
            // if no bytes read, leave
            if (0 == p_task_args->sz_msg_buf)
            {
                // TODO: error
                res = RPLIB_UNSUCCESS;
                goto cleanup;
            }
        }
        else if (p_task_args->epoll_event.events & (EPOLLERR))
        {
            // TODO: error behavior
            res = RPLIB_UNSUCCESS;
        }
        else if (p_task_args->epoll_event.events & (EPOLLHUP))
        {
            // HUP is hang up, connection has closed without being told to,
            // thus..
            res = RPLIB_ERROR;
        }
    }

    // handle message appropriately. If message is expected (e.g. conn state
    // PENDING_STATUS and msg is STATUS, other wise if conn state AVAILABLE)
    // then returns RPLIB_SUCCESS
    res = rpchat_handle_msg(p_task_args->p_conn_queue,
                            p_tpool,
                            p_conn_info,
                            p_task_args->p_msg_buf);
    if (RPLIB_SUCCESS == res)
    {
        res = RPLIB_SUCCESS;
        goto cleanup;
    }
    else
    {
        goto leave;
    }

cleanup:
    free(p_task_args->p_msg_buf);
    p_task_args->p_msg_buf = NULL;
leave:
    return res;
}

/**
 * Helper function for `rpchat_task_conn_proc_event`, when the event is
 * outbound, send the corresponding message to the current connection
 * @param p_task_args Pointer to parent `rpchat_args_proc_event_t` object
 * @return RPLIB_SUCCESS on success; otherwise RPLIB_UNSUCCESS
 */
static int
rpchat_conn_proc_available_outbound(rpchat_args_proc_event_t *p_task_args)
{
    int                 res          = RPLIB_UNSUCCESS;
    rpchat_msg_type_t   new_msg_type = -1;
    rpchat_conn_info_t *p_conn_info
        = (rpchat_conn_info_t *)p_task_args->epoll_event.data.ptr;

    // get message type
    new_msg_type = rpchat_get_msg_type(p_task_args->p_msg_buf);

    // send appropriate message out
    switch (new_msg_type)
    {
        case RPCHAT_BCP_DELIVER:
            rpchat_send_msg(p_conn_info, p_task_args->p_msg_buf);
            break;
        case RPCHAT_BCP_STATUS:
            rpchat_send_msg(p_conn_info, p_task_args->p_msg_buf);
            break;
        default:
            break;
    }
leave:
    return res;
}

/**
 * Helper function for `rpchat_task_conn_proc_event`, when the connection is in
 * an error state. An error status is sent to sender and the connection will be
 * closed.
 * @param p_task_args Pointer to parent `rpchat_args_proc_event_t` object
 * @return RPLIB_SUCCESS on no issues; otherwise, RPLIB_UNSUCCESS
 */
static int
rpchat_conn_proc_error(rpchat_args_proc_event_t *p_task_args)
{
}

void
rpchat_task_conn_proc_event(void *p_args)
{
    rpchat_args_proc_event_t *p_task_args = NULL;
    rpchat_conn_info_t       *p_conn_info = NULL;
    rplib_tpool_t            *p_tpool     = NULL;
    int                       res         = RPLIB_UNSUCCESS;
    // cast args to access fields
    p_task_args = (rpchat_args_proc_event_t *)p_args;
    p_conn_info = (rpchat_conn_info_t *)p_task_args->epoll_event.data.ptr;
    p_tpool     = p_task_args->p_tpool;

    // attempt lock, otherwise requeue
    if (0 > pthread_mutex_trylock(&p_conn_info->mutex_conn))
    {
        goto requeue;
    }

    // if buffer is not allocated, allocate
    if (NULL == p_task_args->p_msg_buf)
    {
        // freed when args object is freed completion of task (on n iteration)
        p_task_args->p_msg_buf = malloc(RPCHAT_MAX_INCOMING_PKT);
        if (NULL == p_task_args->p_msg_buf)
        {
            // TODO: set error
            perror("malloc");
            goto cleanup;
        }
    }

    switch (p_conn_info->conn_status)
    {
        case RPCHAT_CONN_AVAILABLE || RPCHAT_CONN_PRE_REGISTER:
            if (RPCHAT_PROC_EVENT_INBOUND == p_task_args->args_type)
            {
                // handle inbound messages (register, status, send)
                res = rpchat_conn_proc_available_inbound(p_task_args);
            }
            else
            {
                // handle outbound messages (deliver, status)
                res = rpchat_conn_proc_available_outbound(p_task_args);
            }
            break;
        case RPCHAT_CONN_PENDING_STATUS:
            // if not inbound, requeue
            if (RPCHAT_PROC_EVENT_INBOUND != p_task_args->args_type)
            {
                goto requeue;
            }
            // if right direction, try to process message
            else
            {
                res = rpchat_conn_proc_available_inbound(p_task_args);
                // if not successful status processing, drop msg and
                // don't requeue
                if (RPLIB_SUCCESS != res)
                {
                    goto cleanup;
                }
                // success; reset to available
                p_conn_info->conn_status = RPCHAT_CONN_AVAILABLE;
                // start listening
                rpchat_toggle_descriptor(
                    p_task_args->h_fd_epoll, &p_task_args->epoll_event, true);
            }
            break;
        case RPCHAT_CONN_ERR:
            // error occurred, send message and begin closing
            break;
        case RPCHAT_CONN_CLOSED:
            // connection is closed, skip
            break;
        default:
            break;
    }
    // on error occurred during switch branching, change state
    if (RPLIB_ERROR == res)
    {
        p_conn_info->conn_status = RPCHAT_CONN_ERR;
    }
    pthread_mutex_unlock(&p_conn_info->mutex_conn);
cleanup:
    free(p_task_args->p_msg_buf);
    p_task_args->p_msg_buf = NULL;
    free(p_args);
    goto leave;
requeue:
    rplib_tpool_enqueue_task(p_tpool, rpchat_task_conn_proc_event, p_args);
leave:
    return;
}

int
rpchat_handle_existing_connection(unsigned int        h_fd_server,
                                  unsigned int        h_fd_epoll,
                                  rplib_ll_queue_t   *p_conn_queue,
                                  struct epoll_event *p_new_event)
{
    int                 res       = RPLIB_UNSUCCESS;
    char               *p_msg_buf = NULL;
    rpchat_conn_info_t *p_current_conn_info;

    p_current_conn_info = (rpchat_conn_info_t *)p_new_event->data.ptr;

    // POLLIN = wants to give data
    // POLLERR = has problem
    // POLLHUP = they gone
    if (p_new_event->events & EPOLLIN)
    {
        // create buffer for received message
        p_msg_buf = malloc(RPCHAT_MAX_INCOMING_PKT);
        if (NULL == p_msg_buf)
        {
            perror("malloc");
            res = RPLIB_ERROR;
            goto cleanup;
        }

        // receive message from client
        res = rpchat_recvmsg(
            p_current_conn_info->h_fd, RPCHAT_MAX_INCOMING_PKT, p_msg_buf);
        if (RPLIB_SUCCESS != res)
        {
            goto cleanup;
        }

        // handle message appropriately
        //        rpchat_handle_msg(p_conn_queue, p_current_conn_info,
        //        p_msg_buf);
    }
    else if (p_new_event->events & EPOLLHUP)
    {
    }
    else if (p_new_event->events & EPOLLERR)
    {
        res = RPLIB_ERROR;
    }
cleanup:
    free(p_msg_buf);
    p_msg_buf = NULL;
leave:
    return res;
}

int
rpchat_close_connection(rplib_ll_queue_t  *p_conn_queue,
                        rpchat_conn_info_t p_conn_info,
                        unsigned int       h_fd_epoll)
{
    int res = RPLIB_UNSUCCESS;
}

rpchat_msg_type_t
rpchat_get_msg_type(char *p_msg_buf)
{
    int     res    = RPLIB_UNSUCCESS;
    uint8_t opcode = *p_msg_buf;
    switch (opcode)
    {
        case 1:
            res = RPCHAT_BCP_REGISTER;
            break;
        case 2:
            res = RPCHAT_BCP_SEND;
            break;
        case 3:
            res = RPCHAT_BCP_DELIVER;
            break;
        case 4:
            res = RPCHAT_BCP_STATUS;
            break;
        default:
            res = RPLIB_UNSUCCESS;
            break;
    }
    return res;
}

int
rpchat_send_msg(rpchat_conn_info_t *p_sender_info, char *p_msg_buf)
{
    // TODO make call to networking here
    return 0;
}

int
rpchat_handle_msg(rplib_ll_queue_t   *p_conn_queue,
                  rplib_tpool_t      *p_tpool,
                  rpchat_conn_info_t *p_conn_info,
                  char               *p_msg_buf)
{
    int res = RPLIB_UNSUCCESS;

    rpchat_msg_type_t msg_type = rpchat_get_msg_type(p_msg_buf);
    // get type
    // server will only receive RPCHAT_BCP_REGISTER, RPCHAT_BCP_STATUS, and
    // RPCHAT_BCP_SEND messages
    switch (msg_type)
    {
        case RPCHAT_BCP_REGISTER:
            // if registration fails, return ERROR to close connection
            res = RPLIB_SUCCESS
                          == rpchat_handle_register(
                              p_conn_queue, p_tpool, p_conn_info, p_msg_buf)
                      ? RPLIB_SUCCESS
                      : RPLIB_ERROR;
            break;
        case RPCHAT_BCP_SEND:
            res = rpchat_handle_send(
                p_conn_queue, p_tpool, p_conn_info, p_msg_buf);
            break;
        case RPCHAT_BCP_STATUS:
            // returns unsuccess if not looking for status
            res = rpchat_handle_status(p_conn_info, p_msg_buf);
            break;
        default:
            res = RPLIB_ERROR;
            break;
    }
    // TODO: send neg status on failure to handle

leave:
    return res;
}

rpchat_conn_info_t *
rpchat_find_by_username(rplib_ll_queue_t *p_conn_queue,
                        rpchat_string_t  *p_tgt_username)
{
    rpchat_conn_info_t    *p_found_info = NULL; // result
    size_t                 conn_index   = 0;    // index for conn loop
    size_t                 cmp_size     = 0;    // strncmp size per iteration
    int                    cmp_res      = 0;    // strncmp result
    rplib_ll_queue_node_t *p_tgt_node   = NULL; // current node for conn loop
    rpchat_conn_info_t    *p_tgt_info   = NULL; // current info for conn loop

    p_tgt_node = p_conn_queue->p_front;
    for (conn_index; conn_index < p_conn_queue->size; conn_index++)
    {
        p_tgt_info = ((rpchat_conn_info_t *)p_tgt_node->p_data);
        // if lengths are not the same, cannot be same username
        if (p_tgt_info->username.len != p_tgt_username->len)
        {
            continue;
        }
        // compare
        if (0
            == strncmp(p_tgt_info->username.contents,
                       p_tgt_username->contents,
                       cmp_size))
        {
            // on success, this is it
            p_found_info = p_tgt_info;
            goto leave;
        }
    }

leave:
    return p_found_info;
}

/**
 * Sanitize a string to only printable characters
 * @param p_input_string Pointer to input string
 * @param p_output_string Pointer to string to store output
 * @param b_allow_ctrl If true, allow control characters like \n,\t,\w;
 * otherwise, only match printable ascii (excl. space)
 * @return RPLIB_SUCCESS if no issues; otherwise, RPLIB_UNSUCCESS
 */
static int
rpchat_string_sanitize(rpchat_string_t *p_input_string,
                       rpchat_string_t *p_output_string,
                       bool             b_allow_ctrl)
{
    int    res               = RPLIB_UNSUCCESS;
    size_t curr_output_index = 0;
    size_t loop_index        = 0;
    char   curr_char         = 0;
    // double check lengths compliant
    p_input_string->len = p_input_string->len < RPCHAT_MAX_STR_LENGTH
                              ? p_input_string->len
                              : RPCHAT_MAX_STR_LENGTH;
    // ensure output clean
    memset(p_output_string, 0, p_output_string->len);
    for (loop_index = 0; loop_index < p_input_string->len; loop_index++)
    {
        curr_char = p_input_string->contents[loop_index];
        // if character meets filter rules, append to output
        if ((curr_char >= RPCHAT_FILTER_ASCII_START
             && curr_char <= RPCHAT_FILTER_ASCII_END)
            || (b_allow_ctrl
                && (RPCHAT_FILTER_ASCII_TAB == curr_char
                    || RPCHAT_FILTER_ASCII_NEWLINE == curr_char
                    || RPCHAT_FILTER_ASCII_SPACE == curr_char)))
        {
            p_output_string->contents[curr_output_index] = curr_char;
            curr_output_index++;
        }
    }
    // null-terminate
    p_output_string->contents[curr_output_index] = '\0';
    // adjust length
    p_output_string->len = curr_output_index + 1;
    // return unsuccess if string of length 1 (just terminator)
    res = p_output_string->len > 1 ? RPLIB_SUCCESS : RPLIB_UNSUCCESS;

    return res;
}

int
rpchat_handle_register(rplib_ll_queue_t   *p_conn_queue,
                       rplib_tpool_t      *p_tpool,
                       rpchat_conn_info_t *p_conn_info,
                       char               *p_msg_buf)
{
    int                    res        = RPLIB_UNSUCCESS;
    rpchat_pkt_register_t *p_reg_info = NULL;  // msg storage
    rpchat_string_t        new_username;       // original username
    rpchat_string_t        sanitized_username; // stripped username
    rpchat_string_t        reg_msg;            // for other clients

    // check if connection eligible for registration
    if (RPCHAT_CONN_PRE_REGISTER != p_conn_info->conn_status)
    {
        goto leave;
    }

    // cast message buffer as a registration packet struct to acquire fields
    p_reg_info = (rpchat_pkt_register_t *)p_msg_buf;
    // get fields
    p_reg_info->username.len = be16toh(p_reg_info->username.len);
    new_username             = p_reg_info->username;
    res = rpchat_string_sanitize(&new_username, &sanitized_username, false);
    // on sanitization failure, exit
    if (RPLIB_SUCCESS != res)
    {
        goto leave;
    }
    // check for existing username
    if (NULL == rpchat_find_by_username(p_conn_queue, &sanitized_username))
    {
        res = RPLIB_UNSUCCESS;
        goto leave;
    }
    // update connection object
    memcpy(
        &p_conn_info->username, &sanitized_username, sizeof(rpchat_string_t));

    // notify other clients of this registration
    // create message
    reg_msg.len = snprintf(reg_msg.contents,
                           RPCHAT_MAX_STR_LENGTH,
                           "%s has joined the server.",
                           sanitized_username.contents);
    if (0 > reg_msg.len)
    {
        goto leave;
    }
    // make big endian
    reg_msg.len = htobe16(reg_msg.len);
    rpchat_broadcast_msg(p_conn_info, p_conn_queue, p_tpool, &reg_msg);
    // success
    res = RPLIB_SUCCESS;
leave:
    return res;
}

int
rpchat_handle_send(rplib_ll_queue_t   *p_conn_queue,
                   rplib_tpool_t      *p_tpool,
                   rpchat_conn_info_t *p_sender_info,
                   char               *p_msg_buf)
{
    int             res = RPLIB_UNSUCCESS;
    rpchat_string_t curr_msg;

    // assign
    memcpy(&curr_msg,
           &((rpchat_pkt_send_t *)p_msg_buf)->message,
           sizeof(rpchat_string_t));
    // adjust
    curr_msg.len = be16toh(curr_msg.len);
    // send to all
    rpchat_broadcast_msg(p_sender_info, p_conn_queue, p_tpool, &curr_msg);

    return res;
}

int
rpchat_handle_status(rpchat_conn_info_t *p_conn_info, char *p_msg_buf)
{
    int                  res          = RPLIB_UNSUCCESS;
    rpchat_pkt_status_t *p_status_msg = NULL;
    // asserts
    assert(p_conn_info);
    // assignment
    p_status_msg = (rpchat_pkt_status_t *)p_msg_buf;

    // if status is not pending, exit
    if (RPCHAT_CONN_PENDING_STATUS != p_conn_info->conn_status)
    {
        goto leave;
    }
    // handle status
    if (RPCHAT_BCP_STATUS_GOOD == p_status_msg->code)
    {
        res = RPLIB_SUCCESS;
    }
    else
    {
        res = RPLIB_ERROR;
    }

leave:
    return res;
}

int
rpchat_broadcast_msg(rpchat_conn_info_t *p_conn_info,
                     rplib_ll_queue_t   *p_conn_queue,
                     rplib_tpool_t      *p_tpool,
                     rpchat_string_t    *p_msg)
{
    int                       res = RPLIB_UNSUCCESS;
    rpchat_string_t           sanitized_msg;
    rplib_ll_queue_node_t    *p_current_node    = NULL;
    rpchat_args_proc_event_t *p_proc_event_args = NULL;
    rpchat_pkt_deliver_t     *p_new_deliver_msg;

    // sanitize
    rpchat_string_sanitize(p_msg, &sanitized_msg, true);

    // create broadcasts
    p_current_node = p_conn_queue->p_front;
    while (NULL != p_current_node)
    {
        if (p_conn_info != p_current_node->p_data)
        {
            // allocate
            p_proc_event_args = malloc(sizeof(rpchat_args_proc_event_t));
            if (NULL == p_proc_event_args)
            {
                res = RPLIB_ERROR;
                goto cleanup;
            }
            p_proc_event_args->p_msg_buf = malloc(sizeof(rpchat_pkt_deliver_t));
            if (NULL == p_proc_event_args->p_msg_buf)
            {
                res = RPLIB_ERROR;
                goto cleanup;
            }
            p_new_deliver_msg
                = (rpchat_pkt_deliver_t *)p_proc_event_args->p_msg_buf;
            // set fields
            p_proc_event_args->sz_msg_buf   = 0;
            p_proc_event_args->args_type    = RPCHAT_PROC_EVENT_OUTBOUND;
            p_proc_event_args->p_conn_queue = p_conn_queue;
            p_proc_event_args->p_tpool      = p_tpool;
            // copy fields
            p_new_deliver_msg->opcode = RPCHAT_BCP_DELIVER;
            memcpy(&p_new_deliver_msg->from,
                   &p_conn_info->username,
                   sizeof(rpchat_string_t));
            memcpy(&p_new_deliver_msg->message,
                   &sanitized_msg,
                   sizeof(rpchat_string_t));
            // big endian for transit
            p_new_deliver_msg->message.len
                = htobe16(p_new_deliver_msg->message.len);
            p_new_deliver_msg->from.len = htobe16(p_new_deliver_msg->from.len);
            // enqueue
            rplib_tpool_enqueue_task(
                p_tpool, rpchat_task_conn_proc_event, p_proc_event_args);
        }

        p_current_node = p_current_node->p_next_node;
    }
    goto leave;
cleanup:
    free(p_proc_event_args->p_msg_buf);
    p_proc_event_args->p_msg_buf = NULL;
    free(p_proc_event_args);
    p_proc_event_args = NULL;
leave:
    return res;
}
