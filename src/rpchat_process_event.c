/** @file rpchat_process_event.c
 *
 * @brief Implements definitions specific to event handling for basic chat
 * protocol
 *
 * @par
 * COPYRIGHT NOTICE: None
 */

#include "rpchat_process_event.h"

#include "components/rpchat_conn_info.h"

static int
rpchat_conn_proc_handle_inbound_msg(rpchat_args_proc_event_t *p_task_args)
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
            res = rpchat_recv(
                p_conn_info->h_fd, p_task_args->p_msg_buf, sizeof(uint8_t));
            // if no bytes read, leave
            if (0 >= res)
            {
                // TODO: error
                res = RPLIB_ERROR;
                goto cleanup;
            }
            // otherwise, update size
            p_task_args->sz_msg_buf = res;
        }
        else if (p_task_args->epoll_event.events & (EPOLLERR | EPOLLHUP))
        {
            // TODO: error behavior
            res = RPLIB_ERROR;
            goto leave;
        }
    }

    // handle message appropriately. If message is expected (e.g. conn state
    // PENDING_STATUS and msg is STATUS, otherwise if conn state AVAILABLE)
    // then returns RPLIB_SUCCESS
    res = rpchat_handle_msg(p_task_args->p_conn_queue,
                            p_conn_info,
                            p_tpool,
                            p_task_args->p_msg_buf);
    goto leave;

cleanup:
    free(p_task_args->p_msg_buf);
    p_task_args->p_msg_buf = NULL;
leave:
    return res;
} /**
   * Helper function for `rpchat_task_conn_proc_event`, when the event is
   * outbound, send the corresponding message to the current connection
   * @param p_task_args Pointer to parent `rpchat_args_proc_event_t` object
   * @return RPLIB_SUCCESS on success; otherwise RPLIB_UNSUCCESS
   */
static int
rpchat_conn_proc_handle_outbound_msg(rpchat_args_proc_event_t *p_task_args)
{
    int                 res          = RPLIB_UNSUCCESS;
    rpchat_msg_type_t   new_msg_type = -1;
    rpchat_conn_info_t *p_conn_info  = p_task_args->p_conn_info;

    // get message type based on the message stored in buffer
    new_msg_type = rpchat_get_msg_type(p_task_args->p_msg_buf);

    // send appropriate message out
    switch (new_msg_type)
    {
        case RPCHAT_BCP_DELIVER:
            res = rpchat_conn_info_submit_msg(
                p_conn_info, p_task_args->p_msg_buf, p_task_args->sz_msg_buf);
            break;
        case RPCHAT_BCP_STATUS:
            res = rpchat_conn_info_submit_msg(
                p_conn_info, p_task_args->p_msg_buf, p_task_args->sz_msg_buf);
            break;
        default:
            break;
    }
    return res;
} /**
   * Constructs a status message to p_msg buffer in
   * `rpchat_args_proc_event_t` object using passed status code and stat_msg in
   * connection object
   * @param p_task_args Pointer to parent `rpchat_args_proc_event_t` object
   * @return RPLIB_SUCCESS on successful send, RPLIB_ERROR if cannot be sent
   */
static int
rpchat_conn_proc_set_status(rpchat_conn_info_t       *p_recipient_info,
                            rpchat_args_proc_event_t *p_task_args,
                            rpchat_stat_code_t        status_code)
{
    int res = RPLIB_UNSUCCESS;

    int buf_index = 0; // to track memcpy

    // asserts
    assert(NULL != p_task_args->p_msg_buf);

    // copy to args
    // opcode
    *(uint8_t *)p_task_args->p_msg_buf = RPCHAT_BCP_STATUS;
    buf_index += sizeof(uint8_t);
    // status
    *(uint8_t *)(p_task_args->p_msg_buf + buf_index) = status_code;
    buf_index += sizeof(uint8_t);
    // status msg (len)
    memcpy(p_task_args->p_msg_buf + buf_index,
           &p_recipient_info->stat_msg.len,
           sizeof(p_recipient_info->stat_msg.len));
    // big endian
    *(uint16_t *)(p_task_args->p_msg_buf + buf_index)
        = htobe16(p_recipient_info->stat_msg.len);
    buf_index += sizeof(p_recipient_info->stat_msg.len);
    // status msg (contents)
    memcpy(p_task_args->p_msg_buf + buf_index,
           &p_recipient_info->stat_msg.contents,
           p_recipient_info->stat_msg.len);
    buf_index += p_recipient_info->stat_msg.len;
    p_task_args->sz_msg_buf = buf_index;

    // clear message
    p_recipient_info->stat_msg.len = 0;
    res                            = RPLIB_SUCCESS;

    return res;
}

/**
 * Helper function to enqueue a Status message for a given recipient
 * `rpchat_conn_info_t` object to be processed later
 * @param p_recipient_info  Pointer to recipient connection
 * `rpchat_conn_info_t`
 * @param p_conn_queue Pointer to connection Queue
 * @param p_tpool Pointer to threadpool object
 * @param status_code `rpchat_stat_code_t` status code to send
 * @return RPLIB_SUCCESS on no issues, otherwise RPLIB_UNSUCCESS
 */
static int
rpchat_conn_proc_enqueue_status(rpchat_conn_info_t  *p_recipient_info,
                                rpchat_conn_queue_t *p_conn_queue,
                                rplib_tpool_t       *p_tpool,
                                rpchat_stat_code_t   status_code)
{
    int                       res               = RPLIB_UNSUCCESS;
    rpchat_args_proc_event_t *p_proc_event_args = NULL;

    // allocate
    p_proc_event_args = malloc(sizeof(rpchat_args_proc_event_t));
    if (NULL == p_proc_event_args)
    {
        res = RPLIB_ERROR;
        goto cleanup;
    }
    p_proc_event_args->p_msg_buf = malloc(sizeof(rpchat_pkt_status_t));
    if (NULL == p_proc_event_args->p_msg_buf)
    {
        res = RPLIB_ERROR;
        goto cleanup;
    }

    // set fields
    p_proc_event_args->args_type    = RPCHAT_PROC_EVENT_OUTBOUND;
    p_proc_event_args->p_conn_queue = p_conn_queue;
    p_proc_event_args->p_conn_info  = p_recipient_info;
    p_proc_event_args->p_tpool      = p_tpool;
    // create status msg in new proc_events
    res = rpchat_conn_proc_set_status(
        p_recipient_info, p_proc_event_args, status_code);
    if (RPLIB_SUCCESS != res)
    {
        goto cleanup;
    }
    // enqueue
    res = rpchat_conn_info_enqueue_task(p_recipient_info,
                                        p_tpool,
                                        rpchat_task_conn_proc_event,
                                        p_proc_event_args);
    if (RPLIB_SUCCESS != res)
    {
        goto cleanup;
    }
    res = RPLIB_SUCCESS;
    goto leave;
cleanup:
    if (NULL != p_proc_event_args)
    {
        free(p_proc_event_args->p_msg_buf);
        p_proc_event_args->p_msg_buf = NULL;
    }
    free(p_proc_event_args);
    p_proc_event_args = NULL;
leave:
    return res;
}
/**
 * Construct a deliver message in the p_msg buffer
 * within passed `rpchat_args_proc_event_t` object
 * @param p_task_args Pointer to parent `rpchat_args_proc_event_t` object
 * @param p_sender Pointer to `rpchat_string_t` containing sender
 * @param p_msg Pointer to `rpchat_string_t` containing message
 * @return RPLIB_SUCCESS on success, RPLIB_UNSUCCESS on failure
 */
static int
rpchat_conn_proc_set_deliver(rpchat_args_proc_event_t *p_task_args,
                             rpchat_string_t          *p_sender,
                             rpchat_string_t          *p_msg)
{
    int             res       = RPLIB_UNSUCCESS;
    int             buf_index = 0;
    rpchat_string_t sanitized_msg;

    assert(p_sender);
    assert(p_msg);

    rpchat_string_sanitize(p_msg, &sanitized_msg, true);

    buf_index = 0;
    // opcode field
    *p_task_args->p_msg_buf = RPCHAT_BCP_DELIVER;
    buf_index += sizeof(uint8_t);
    // from field (len)
    memcpy(p_task_args->p_msg_buf + buf_index,
           &p_sender->len,
           sizeof(p_sender->len));
    // big endian
    *(uint16_t *)(p_task_args->p_msg_buf + buf_index) = htobe16(p_sender->len);
    buf_index += sizeof(p_sender->len);
    // from field (contents)
    memcpy(
        p_task_args->p_msg_buf + buf_index, &p_sender->contents, p_sender->len);
    buf_index += p_sender->len;
    // msg field (len)
    memcpy(p_task_args->p_msg_buf + buf_index,
           &sanitized_msg.len,
           sizeof(sanitized_msg.len));
    // big endian
    *(uint16_t *)(p_task_args->p_msg_buf + buf_index)
        = htobe16(sanitized_msg.len);
    buf_index += sizeof(sanitized_msg.len);
    // msg field (contents)
    memcpy(p_task_args->p_msg_buf + buf_index,
           &sanitized_msg.contents,
           sanitized_msg.len);
    buf_index += sanitized_msg.len;
    // update size
    p_task_args->sz_msg_buf = buf_index;
    res                     = RPLIB_SUCCESS;

    return res;
}
/**
 * Helper function to enqueue a Deliver message for a given recipient
 * `rpchat_conn_info_t` object to be processed later
 * @param p_recipient_info  Pointer to recipient connection
 * `rpchat_conn_info_t`
 * @param p_conn_queue Pointer to Connection Queue
 * @param p_tpool Pointer to threadpool object
 * @param p_sender_str Pointer to string of "sender"
 * @param p_msg Pointer to string of message to send
 * @return RPLIB_SUCCESS on no issues, otherwise RPLIB_UNSUCCESS
 */
static int
rpchat_conn_proc_enqueue_deliver(rpchat_conn_info_t  *p_recipient_info,
                                 rpchat_conn_queue_t *p_conn_queue,
                                 rplib_tpool_t       *p_tpool,
                                 rpchat_string_t     *p_sender_str,
                                 rpchat_string_t     *p_msg)
{
    int                       res               = RPLIB_UNSUCCESS;
    rpchat_args_proc_event_t *p_proc_event_args = NULL;

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

    // set fields
    p_proc_event_args->args_type    = RPCHAT_PROC_EVENT_OUTBOUND;
    p_proc_event_args->p_conn_queue = p_conn_queue;
    p_proc_event_args->p_conn_info  = p_recipient_info;
    p_proc_event_args->p_tpool      = p_tpool;
    // create deliver msg in new proc_events
    res = rpchat_conn_proc_set_deliver(p_proc_event_args, p_sender_str, p_msg);
    if (RPLIB_SUCCESS != res)
    {
        goto cleanup;
    }
    // enqueue
    res = rpchat_conn_info_enqueue_task(p_recipient_info,
                                        p_tpool,
                                        rpchat_task_conn_proc_event,
                                        p_proc_event_args);
    if (RPLIB_SUCCESS != res)
    {
        goto cleanup;
    }
    res = RPLIB_SUCCESS;
    goto leave;
cleanup:
    if (NULL != p_proc_event_args)
    {
        free(p_proc_event_args->p_msg_buf);
        p_proc_event_args->p_msg_buf = NULL;
    }
    free(p_proc_event_args);
    p_proc_event_args = NULL;
leave:
    return res;
}
/**
 * Helper function for `rpchat_task_conn_proc_event`, when the connection is
 * in an error state. An error status is sent to sender and the connection
 * is closed.
 * @param p_task_args Pointer to parent `rpchat_args_proc_event_t` object
 * @return RPLIB_SUCCESS on no issues; otherwise, RPLIB_UNSUCCESS
 */
static int
rpchat_conn_proc_error(rpchat_args_proc_event_t *p_task_args)
{
    int                 res         = RPLIB_UNSUCCESS;
    rpchat_conn_info_t *p_conn_info = NULL;

    p_conn_info = (rpchat_conn_info_t *)p_task_args->p_conn_info;

    // set message
    rpchat_conn_proc_set_status(
        p_conn_info, p_task_args, RPCHAT_BCP_STATUS_ERROR);

    // attempt to send status
    rpchat_conn_info_submit_msg(
        p_conn_info, p_task_args->p_msg_buf, p_task_args->sz_msg_buf);

    // stop listening and close socket(s)
    res = rpchat_close_connection(p_task_args->p_conn_queue->h_fd_epoll,
                                  p_conn_info->h_fd);

    // get out
    return res;
}

void
rpchat_task_conn_proc_event(void *p_args)
{
    rpchat_args_proc_event_t *p_task_args = NULL;
    rpchat_conn_info_t       *p_conn_info = NULL;
    rplib_tpool_t            *p_tpool     = NULL;
    int                       res         = RPLIB_SUCCESS;
    rpchat_string_t           dc_msg;
    // cast args to access fields
    p_task_args = (rpchat_args_proc_event_t *)p_args;
    p_conn_info = (rpchat_conn_info_t *)p_task_args->p_conn_info;
    p_tpool     = p_task_args->p_tpool;

    // update queue on conn info
    atomic_fetch_sub(&p_conn_info->pending_jobs, 1);

    // attempt lock, otherwise requeue
    if (RPLIB_SUCCESS != pthread_mutex_trylock(&p_conn_info->mutex_conn))
    {
        goto requeue_no_unlock;
    }

    // update last activity if not a HEARTBEAT event
    if(RPCHAT_PROC_EVENT_HEARTBEAT != p_task_args->args_type)
    {
        p_conn_info->last_active = time(0);
    }

    // if buffer is not allocated, allocate
    if (NULL == p_task_args->p_msg_buf)
    {
        // freed when args object is freed completion of task (on n
        // iteration)
        p_task_args->p_msg_buf = malloc(RPCHAT_MAX_INCOMING_MSG);
        if (NULL == p_task_args->p_msg_buf)
        {
            // TODO: set error
            perror("malloc");
            goto cleanup;
        }
    }

    // event is HEARTBEAT, check how long inactive for
    // if not already CONN_CLOSING or CONN_ERR
    if (RPCHAT_PROC_EVENT_HEARTBEAT == p_task_args->args_type
        && (RPCHAT_CONN_CLOSING != p_conn_info->conn_status
            && RPCHAT_CONN_ERR != p_conn_info->conn_status))
    {
        if(RPCHAT_CONNECTION_TIMEOUT>time(0) - p_conn_info->last_active){
            p_conn_info->stat_msg.len = snprintf(p_conn_info->stat_msg.contents,
                                                 RPCHAT_MAX_STR_LENGTH,
                                                 "Disconnected for inactivity.");
            p_conn_info->conn_status  = RPCHAT_CONN_ERR;
        }
    }

    switch (p_conn_info->conn_status)
    {
        case RPCHAT_CONN_PRE_REGISTER:
            // drop down
        case RPCHAT_CONN_AVAILABLE:
            if (RPCHAT_PROC_EVENT_INBOUND == p_task_args->args_type)
            {
                // handle messages inbound from client (register, status,
                // send)
                res = rpchat_conn_proc_handle_inbound_msg(p_task_args);

                // on success, requeue to send status
                if (RPLIB_SUCCESS == res)
                {
                    p_conn_info->conn_status = RPCHAT_CONN_SEND_STAT;
                    rpchat_conn_proc_enqueue_status(p_conn_info,
                                                    p_task_args->p_conn_queue,
                                                    p_tpool,
                                                    RPCHAT_BCP_STATUS_GOOD);
                }
                // on unsuccess, kill connection TODO: is this overkill?
                else
                {
                    res = RPLIB_ERROR;
                }
                break;
            }
            // if AVAILABLE and event is OUTBOUND, state change and requeue
            // to process on next pass
            else
            {
                p_conn_info->conn_status = RPCHAT_CONN_SEND_MSG;
                goto requeue;
            }
        case RPCHAT_CONN_SEND_STAT:
            // if event is not outbound and not stat, requeue
            if (RPCHAT_PROC_EVENT_OUTBOUND != p_task_args->args_type
                || RPCHAT_BCP_STATUS
                       != rpchat_get_msg_type(p_task_args->p_msg_buf))
            {
                goto requeue;
            }
            res = rpchat_conn_proc_handle_outbound_msg(p_task_args);
            // go back to AVAILABLE once status has been sent
            if (RPLIB_SUCCESS == res)
            {
                p_conn_info->conn_status = RPCHAT_CONN_AVAILABLE;
                // start listening
                rpchat_toggle_descriptor(p_task_args->p_conn_queue->h_fd_epoll,
                                         p_conn_info->h_fd,
                                         p_conn_info,
                                         true);
            }
            break;
        case RPCHAT_CONN_SEND_MSG:
            // sending outbound message (deliver, status)
            // if event is not outbound, requeue
            if (RPCHAT_PROC_EVENT_OUTBOUND != p_task_args->args_type)
            {
                goto requeue;
            }
            // if message is outbound, send it to client
            res = rpchat_conn_proc_handle_outbound_msg(p_task_args);
            // wait for status if sent successfully
            if (RPLIB_SUCCESS == res)
            {
                p_conn_info->conn_status = RPCHAT_CONN_PENDING_STATUS;
                // start listening
                rpchat_toggle_descriptor(p_task_args->p_conn_queue->h_fd_epoll,
                                         p_conn_info->h_fd,
                                         p_conn_info,
                                         true);
            }
            break;
        case RPCHAT_CONN_PENDING_STATUS:
            // if not inbound, requeue. Need a STATUS before sending
            // anything.
            if (RPCHAT_PROC_EVENT_INBOUND != p_task_args->args_type)
            {
                goto requeue;
            }
            // if message inbound from client, try to process message
            else
            {
                // while in PENDING_STATUS state this
                // call will return RPLIB_UNSUCCESS if msg is not STATUS
                res = rpchat_conn_proc_handle_inbound_msg(p_task_args);
                // if error, set error state and requeue to handle
                if (RPLIB_ERROR == res)
                {
                    p_conn_info->conn_status = RPCHAT_CONN_ERR;
                    goto requeue;
                }
                // success; reset to available
                p_conn_info->conn_status = RPCHAT_CONN_AVAILABLE;
                // start listening
                rpchat_toggle_descriptor(p_task_args->p_conn_queue->h_fd_epoll,
                                         p_conn_info->h_fd,
                                         p_conn_info,
                                         true);
            }
            break;
        case RPCHAT_CONN_ERR:
            // error occurred, send message and begin closing
            rpchat_conn_proc_error(p_args);
            p_conn_info->conn_status = RPCHAT_CONN_CLOSING;
            goto requeue;
        case RPCHAT_CONN_CLOSING:
            // connection is closing...waiting for final out
            if (0 == atomic_load(&p_conn_info->pending_jobs))
            {
                // notify
                dc_msg.len = snprintf(dc_msg.contents,
                                      RPCHAT_MAX_STR_LENGTH,
                                      "%s has left the server.",
                                      (0 < p_conn_info->username.len)
                                          ? p_conn_info->username.contents
                                          : "An unregistered user");

                rpchat_broadcast_msg(p_task_args->p_conn_queue,
                                     p_conn_info,
                                     &p_task_args->p_conn_queue->server_str,
                                     p_tpool,
                                     &dc_msg);

                rpchat_conn_queue_destroy_conn_info(p_task_args->p_conn_queue,
                                                    p_conn_info);

                goto cleanup_no_unlock;
            }
        default:
            break;
    }
    // on error occurred during switch branching, change state
    if (RPLIB_ERROR == res)
    {
        p_conn_info->conn_status = RPCHAT_CONN_ERR;
        goto requeue;
    }
cleanup:
    pthread_mutex_unlock(&p_conn_info->mutex_conn);
cleanup_no_unlock:
    free(p_task_args->p_msg_buf);
    p_task_args->p_msg_buf = NULL;
    free(p_args);
    goto leave;
requeue:
    pthread_mutex_unlock(&p_conn_info->mutex_conn);
requeue_no_unlock:
    // only requeue if not closing
    if (!atomic_load(&p_tpool->b_terminate))
    {
        rpchat_conn_info_enqueue_task(
            p_conn_info, p_tpool, rpchat_task_conn_proc_event, p_args);
    }
leave:
    return;
}

int
rpchat_handle_msg(rpchat_conn_queue_t           *p_conn_queue,
                  struct rpchat_connection_info *p_conn_info,
                  rplib_tpool_t                 *p_tpool,
                  char                          *p_msg_buf)
{
    int res = RPLIB_ERROR;

    enum rpchat_message_type msg_type = rpchat_get_msg_type(p_msg_buf);
    // get type
    // server will only receive RPCHAT_BCP_REGISTER, RPCHAT_BCP_STATUS, and
    // RPCHAT_BCP_SEND messages
    switch (msg_type)
    {
        case RPCHAT_BCP_REGISTER:
            // if registration fails, return ERROR to close connection
            res = RPLIB_SUCCESS
                          == rpchat_handle_register(
                              p_conn_queue, p_conn_info, p_tpool)
                      ? RPLIB_SUCCESS
                      : RPLIB_ERROR;
            break;
        case RPCHAT_BCP_SEND:
            res = rpchat_handle_send(p_conn_queue, p_conn_info, p_tpool);
            break;
        case RPCHAT_BCP_STATUS:
            // returns unsuccess if not looking for status
            res = rpchat_conn_info_handle_status(p_conn_info);
            break;
        default:
            res = RPLIB_ERROR;
            break;
    }
    // TODO: set neg status on failure to handle
    return res;
}

int
rpchat_handle_register(rpchat_conn_queue_t           *p_conn_queue,
                       struct rpchat_connection_info *p_conn_info,
                       rplib_tpool_t                 *p_tpool)
{
    int                   res = RPLIB_UNSUCCESS;
    rpchat_pkt_register_t reg_info;           // msg storage
    rpchat_string_t       new_username;       // original username
    rpchat_string_t       sanitized_username; // stripped username
    rpchat_string_t       group_reg_msg;      // for other clients
    rpchat_string_t       client_reg_msg;     // for this client

    // check if connection eligible for registration
    if (RPCHAT_CONN_PRE_REGISTER != p_conn_info->conn_status)
    {
        goto leave;
    }

    // get fields
    // username length
    if (sizeof(reg_info.username.len)
        != rpchat_recv(p_conn_info->h_fd,
                       (char *)&reg_info.username.len,
                       sizeof(reg_info.username.len)))
    {
        goto leave;
    }
    reg_info.username.len = be16toh(reg_info.username.len);
    // if larger than authorized, kill
    if (RPCHAT_MAX_STR_LENGTH < reg_info.username.len)
    {
        goto leave;
    }
    // username
    if (reg_info.username.len
        != rpchat_recv(p_conn_info->h_fd,
                       (char *)&reg_info.username.contents,
                       reg_info.username.len))
    {
        goto leave;
    }
    new_username = reg_info.username;
    res = rpchat_string_sanitize(&new_username, &sanitized_username, false);
    // on sanitization failure, exit
    if (RPLIB_SUCCESS != res)
    {
        goto leave;
    }
    // check for existing username. Want NULL
    if (NULL
        != rpchat_conn_queue_find_by_username(p_conn_queue,
                                              &sanitized_username))
    {
        res = RPLIB_UNSUCCESS;
        goto leave;
    }
    // update connection object
    memcpy(
        &p_conn_info->username, &sanitized_username, sizeof(rpchat_string_t));

    // notify other clients of this registration
    // create message
    group_reg_msg.len = snprintf(group_reg_msg.contents,
                                 RPCHAT_MAX_STR_LENGTH,
                                 "%s has joined the server.",
                                 sanitized_username.contents);

    client_reg_msg.len = snprintf(client_reg_msg.contents,
                                  RPCHAT_MAX_STR_LENGTH,
                                  "Logged in as %s.\nCurrent Clients: \n",
                                  p_conn_info->username.contents);
    if (1 < p_conn_queue->p_conn_ll->size)
    {
        rpchat_conn_queue_list_users(p_conn_queue, &client_reg_msg);
    }

    // enqueue message with registering client
    rpchat_conn_proc_enqueue_deliver(p_conn_info,
                                     p_conn_queue,
                                     p_tpool,
                                     &p_conn_queue->server_str,
                                     &client_reg_msg);

    // send to all
    rpchat_broadcast_msg(p_conn_queue,
                         p_conn_info,
                         &p_conn_queue->server_str,
                         p_tpool,
                         &group_reg_msg);
    // success
    res = RPLIB_SUCCESS;
leave:
    return res;
}

int
rpchat_handle_send(rpchat_conn_queue_t           *p_conn_queue,
                   struct rpchat_connection_info *p_sender_info,
                   rplib_tpool_t                 *p_tpool)
{
    int             res = RPLIB_UNSUCCESS;
    rpchat_string_t curr_msg;

    // get rest of message
    // message length
    if (sizeof(curr_msg.len)
        != rpchat_recv(
            p_sender_info->h_fd, (char *)&curr_msg.len, sizeof(curr_msg.len)))
    {
        goto leave;
    }
    curr_msg.len = be16toh(curr_msg.len);
    // if larger than authorized, kill
    if (RPCHAT_MAX_STR_LENGTH < curr_msg.len)
    {
        goto leave;
    }
    // message
    if (curr_msg.len
        != rpchat_recv(
            p_sender_info->h_fd, (char *)&curr_msg.contents, curr_msg.len))
    {
        goto leave;
    }
    // send to all
    res = rpchat_broadcast_msg(p_conn_queue,
                               p_sender_info,
                               &p_sender_info->username,
                               p_tpool,
                               &curr_msg);
leave:
    return res;
}

int
rpchat_broadcast_msg(rpchat_conn_queue_t           *p_conn_queue,
                     struct rpchat_connection_info *p_sender_info,
                     rpchat_string_t               *p_sender_str,
                     rplib_tpool_t                 *p_tpool,
                     rpchat_string_t               *p_msg)
{
    int                            res = RPLIB_UNSUCCESS;
    rpchat_string_t                sanitized_msg;
    struct rplib_ll_queue_node    *p_current_node = NULL;
    struct rpchat_connection_info *p_current_info = NULL;
    int                            index          = 0;

    // sanitize
    rpchat_string_sanitize(p_msg, &sanitized_msg, true);

    // if no passed username, use the calling conn_info username
    if (!p_sender_str)
    {
        p_sender_str = &p_sender_info->username;
    }

    // logging
    printf("%s: %s\n", p_sender_str->contents, sanitized_msg.contents);

    // create broadcasts
    assert(NULL != &p_conn_queue->mutex_conn_ll);
    pthread_mutex_lock(&p_conn_queue->mutex_conn_ll);
    p_current_node = p_conn_queue->p_conn_ll->p_front;
    while (NULL != p_current_node)
    {
        if (p_sender_info != p_current_node->p_data)
        {
            p_current_info
                = (struct rpchat_connection_info *)p_current_node->p_data;
            // if closing or in error state, skip
            if (RPCHAT_CONN_CLOSING == p_current_info->conn_status
                || RPCHAT_CONN_ERR == p_current_info->conn_status)
            {
                continue;
            }
            rpchat_conn_proc_enqueue_deliver(p_current_info,
                                             p_conn_queue,
                                             p_tpool,
                                             p_sender_str,
                                             &sanitized_msg);
        }
        index++;
        p_current_node = p_current_node->p_next_node;
    }
    res = RPLIB_SUCCESS;

    pthread_mutex_unlock(&p_conn_queue->mutex_conn_ll);
    return res;
}
int
rpchat_conn_info_handle_status(rpchat_conn_info_t *p_conn_info)
{
    int                 res = RPLIB_UNSUCCESS;
    rpchat_pkt_status_t status_msg;
    // asserts
    assert(p_conn_info);

    // if status is not pending, exit
    if (RPCHAT_CONN_PENDING_STATUS != p_conn_info->conn_status)
    {
        goto leave;
    }

    // get rest of message
    // status code
    if (sizeof(status_msg.code)
        != rpchat_recv(p_conn_info->h_fd,
                       (char *)&status_msg.code,
                       sizeof(status_msg.code)))
    {
        goto leave;
    }

    // handle status
    if (RPCHAT_BCP_STATUS_GOOD == status_msg.code)
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
rpchat_conn_info_submit_msg(rpchat_conn_info_t *p_sender_info,
                            char               *p_msg_buf,
                            size_t              sz_msg_buf)
{
    int res = RPLIB_UNSUCCESS;

    // send message
    res = 0 > rpchat_sendmsg(p_sender_info->h_fd, p_msg_buf, sz_msg_buf)
              ? RPLIB_UNSUCCESS
              : RPLIB_SUCCESS;
    // if sendmsg fails, set error state
    if (RPLIB_UNSUCCESS == res)
    {
        p_sender_info->conn_status = RPCHAT_CONN_ERR;
    }

    return res;
}