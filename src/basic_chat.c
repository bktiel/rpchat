#include "basic_chat.h"

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

int
rpchat_handle_events(struct epoll_event *p_ret_event_buf,
                     int                 h_fd_server,
                     int                 h_fd_epoll,
                     unsigned int        max_connections,
                     rplib_ll_queue_t   *p_conn_queue)
{
    int          res         = RPLIB_UNSUCCESS;
    int          h_new_fd    = RPLIB_ERROR;
    unsigned int event_index = 0; // index for event loop

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
        // process existing connection
        rpchat_handle_existing_connection(h_fd_server,
                                          h_fd_epoll,
                                          p_conn_queue,
                                          &p_ret_event_buf[event_index]);
    }
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
    rpchat_conn_info_t     p_new_conn_info;
    struct epoll_event     new_event;

    // accept connection
    h_new_fd = rpchat_accept_new_connection(h_fd_server);
    if (0 > h_new_fd)
    {
        goto leave;
    }

    // set fields
    p_new_conn_info.h_fd = h_new_fd;
    // enqueue new conn info
    p_new_node = rplib_ll_queue_enqueue(
        p_conn_queue, &p_new_conn_info, sizeof(rpchat_conn_info_t));
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
        rpchat_handle_msg(p_conn_queue, p_current_conn_info, p_msg_buf);
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
rpchat_handle_msg(rplib_ll_queue_t   *p_conn_queue,
                  rpchat_conn_info_t *p_sender_info,
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
            res = rpchat_handle_register(
                p_conn_queue, p_sender_info, p_msg_buf);
            break;
        case RPCHAT_BCP_SEND:
            res = rpchat_handle_send(p_conn_queue, p_sender_info, p_msg_buf);
            break;
        case RPCHAT_BCP_STATUS:
            res = rpchat_handle_status(p_sender_info, p_msg_buf);
            break;
        default:
            break;
    }
    // TODO: send neg status on failure to handle

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
                       rpchat_conn_info_t *p_sender_info,
                       char               *p_msg_buf)
{
    int                    res        = RPLIB_UNSUCCESS;
    rpchat_pkt_register_t *p_reg_info = NULL; // msg storage
    rpchat_string_t        new_username;
    rpchat_string_t        sanitized_username;

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
        &p_sender_info->username, &sanitized_username, sizeof(rpchat_string_t));
    res = RPLIB_SUCCESS;

    // prime other connections for

leave:
    return res;
}

int
rpchat_handle_send(rplib_ll_queue_t   *p_conn_queue,
                   rpchat_conn_info_t *p_sender_info,
                   char               *p_msg_buf)
{
}

int
rpchat_handle_status(rpchat_conn_info_t *p_sender_info, char *p_msg_buf)
{
}