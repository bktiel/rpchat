/** @file rpchat_conn_queue.c
 *
 * @brief Implements definitions specific to provided basic chat protocol
 *
 * @par
 * COPYRIGHT NOTICE: None
 */

#include "components/rpchat_conn_queue.h"

rpchat_conn_queue_t *
rpchat_conn_queue_create(int h_fd_epoll)
{
    rpchat_conn_queue_t *p_conn_queue = NULL;

    // allocate
    p_conn_queue = malloc(sizeof(rpchat_conn_queue_t));
    if (!p_conn_queue)
    {
        goto cleanup;
    }

    // store a "server" identifier for use on server messages
    p_conn_queue->server_str.len = snprintf(p_conn_queue->server_str.contents,
                                            RPCHAT_MAX_STR_LENGTH,
                                            "%s",
                                            RPCHAT_SERVER_IDENTIFIER);

    // initialize children
    p_conn_queue->p_conn_ll = rplib_ll_queue_create();
    if (!p_conn_queue->p_conn_ll)
    {
        goto cleanup;
    }
    p_conn_queue->h_fd_epoll = h_fd_epoll;
    pthread_mutex_init(&(p_conn_queue->mutex_conn_ll), NULL);
    goto leave;
cleanup:
    free(p_conn_queue);
    p_conn_queue = NULL;
leave:
    return p_conn_queue;
}
int
rpchat_conn_queue_destroy(rpchat_conn_queue_t *p_conn_queue)
{
    pthread_mutex_destroy(&p_conn_queue->mutex_conn_ll);
    rplib_ll_queue_destroy(p_conn_queue->p_conn_ll);
    free(p_conn_queue);
    p_conn_queue = NULL;

    return RPLIB_SUCCESS;
}
int
rpchat_conn_queue_destroy_conn_info(rpchat_conn_queue_t *p_conn_queue,
                                    rpchat_conn_info_t  *p_conn_info)
{
    int                         res        = RPLIB_UNSUCCESS;
    struct rplib_ll_queue_node *p_tgt_node = p_conn_queue->p_conn_ll->p_front;

    // destroy mutex
    pthread_mutex_unlock(&p_conn_info->mutex_conn);
    pthread_mutex_destroy(&p_conn_info->mutex_conn);

    // delete object
    pthread_mutex_lock(&p_conn_queue->mutex_conn_ll);
    while (NULL != p_tgt_node)
    {
        // if match with address, remove
        if (p_tgt_node->p_data == p_conn_info)
        {
            // remove
            rplib_ll_remove_node(p_conn_queue->p_conn_ll, p_tgt_node);
            res = RPLIB_SUCCESS;
            goto leave;
        }
        p_tgt_node = p_tgt_node->p_next_node;
    }

leave:
    pthread_mutex_unlock(&p_conn_queue->mutex_conn_ll);
    return res;
}

rpchat_conn_info_t *
rpchat_conn_queue_find_by_username(rpchat_conn_queue_t *p_conn_queue,
                                   rpchat_string_t     *p_tgt_username)
{
    rpchat_conn_info_t    *p_found_info = NULL; // result
    size_t                 conn_index   = 0;    // index for conn loop
    size_t                 cmp_size     = 0;    // strncmp size per iteration
    rplib_ll_queue_node_t *p_tgt_node   = NULL; // current node for conn loop
    rpchat_conn_info_t    *p_tgt_info   = NULL; // current info for conn loop

    pthread_mutex_lock(&p_conn_queue->mutex_conn_ll);
    p_tgt_node = p_conn_queue->p_conn_ll->p_front;
    for (conn_index = 0; conn_index < p_conn_queue->p_conn_ll->size;
         conn_index++)
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
    pthread_mutex_unlock(&p_conn_queue->mutex_conn_ll);
    return p_found_info;
}

int
rpchat_conn_queue_list_users(rpchat_conn_queue_t *p_conn_queue,
                             rpchat_string_t     *p_output_buf)
{
    int                    res         = RPLIB_UNSUCCESS;
    rplib_ll_queue_node_t *p_curr_node = NULL;
    rpchat_conn_info_t    *p_curr_info = NULL;
    int                    buf_index   = p_output_buf->len - 1;
    const char            *fmt;

    pthread_mutex_lock(&p_conn_queue->mutex_conn_ll);
    p_curr_node = p_conn_queue->p_conn_ll->p_front;
    while (p_curr_node != NULL)
    {
        p_curr_info = (rpchat_conn_info_t *)p_curr_node->p_data;
        // if not front, append comma
        fmt = p_conn_queue->p_conn_ll->p_front == p_curr_node ? "%s" : ", %s";
        buf_index += snprintf((char *)p_output_buf->contents + buf_index,
                              RPCHAT_MAX_STR_LENGTH - buf_index,
                              fmt,
                              p_curr_info->username.contents);
        p_curr_node = p_curr_node->p_next_node;
    }
    // update length
    p_output_buf->len = buf_index;
    pthread_mutex_unlock(&p_conn_queue->mutex_conn_ll);
    res = RPLIB_SUCCESS;
    return res;
}
