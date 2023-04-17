/** @file conn_map.c
 *
 * @brief Connection map used for all incoming clients
 *
 * @par
 * COPYRIGHT NOTICE: None
 */
#include "conn_tracker.h"

#include <assert.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include "io.h"
#include "queue_ll_lib.h"
#include "rp_common.h"

/**
 * Helper function used as a callback by vector to close pollfd objects
 * @param p_pollfd
 */
void
rptftp_close_pollfd(struct pollfd *p_pollfd)
{
    int h_fd = RPLIB_GET_ABSOLUTE(p_pollfd->fd);
    close(h_fd);
    // space in buffer is re-used so clear on cleanup
    memset(&p_pollfd, 0, sizeof(struct pollfd));
}

/**
 * Helper function used as a callback by vector to close conn_meta objects.\n
 * Note: Creates memory allocation for meta that must be freed manually or
 * close_
 * @param p_pollfd
 */
void
rptftp_close_meta(rptftp_conn_meta_t *p_conn_meta)
{
    pthread_mutex_lock(&p_conn_meta->usage_mut);
    pthread_mutex_unlock(&p_conn_meta->usage_mut);
    pthread_mutex_destroy(&p_conn_meta->usage_mut);
    // space in buffer is re-used so clear on cleanup
    // memset(p_conn_meta, 0, sizeof(calc_conn_meta_t));
    free(p_conn_meta);
    p_conn_meta = NULL;
}

/**
 * Initialize a connection map with underlying structures
 * @param p_conn_map Pointer to conn_map object to initialize
 * @return 0 on success, 1 on failure
 */
int
rptftp_conn_map_initialize(rptftp_conn_map_t *p_conn_map, size_t timeout)
{
    assert(p_conn_map);
    assert(timeout);
    int res = RPLIB_UNSUCCESS;
    // create conn map and initialize
    p_conn_map->vec_conn_pollfd = vector_create(
        sizeof(struct pollfd), 32, (void (*)(void *))rptftp_close_pollfd);
    p_conn_map->list_conn_meta = queue_ll_create_queue();
    // failure
    if (!p_conn_map->list_conn_meta || !p_conn_map->vec_conn_pollfd)
    {
        res = RPLIB_ERROR;
        vector_destroy(&p_conn_map->vec_conn_pollfd);
        queue_ll_destroy(&p_conn_map->list_conn_meta);
        goto leave;
    }
    // success
    res                 = RPLIB_SUCCESS;
    p_conn_map->size    = 0;
    p_conn_map->timeout = timeout;
leave:
    return res;
}

/**
 * Destroy a given connection map and underlying structures
 * @return
 */
int
rptftp_conn_map_destroy(rptftp_conn_map_t *p_conn_map)
{
    struct queue_ll_node *p_current_node = NULL;
    assert(p_conn_map);

    // attempt to clear list of any lingering conn objects
    p_current_node = p_conn_map->list_conn_meta->p_front;
    while (NULL != p_current_node)
    {
        rptftp_close_meta(p_current_node->p_data);
        p_current_node = p_current_node->p_next;
    }
    // destroy nodes
    queue_ll_destroy(&p_conn_map->list_conn_meta);
    // clean vectors
    vector_clear(p_conn_map->vec_conn_pollfd);
    if (vector_destroy(&p_conn_map->vec_conn_pollfd))
    {
        RPLIB_DEBUG_PRINTF("Error: %s.\n",
                     "Failed to clean up netcalc conn sdbuf vector");
    }
    // destroy descriptor
    rplib_close_dir(p_conn_map->p_dirstream);
    return RPLIB_SUCCESS;
}

/**
 * Adds a connection to a given conn map
 * @param p_conn_map Object to act on
 * @param h_sd int open socket descriptor
 * @return 0 on success, 1 on failure
 */
int
rptftp_conn_map_add_conn(rptftp_conn_map_t *p_conn_map, int h_sd)
{
    int               res = 1;
    struct pollfd     new_pollfd;
    rptftp_conn_meta_t *p_new_meta = NULL;

    assert(p_conn_map);

    // assignments
    memset(&new_pollfd, 0, sizeof(struct pollfd));
    new_pollfd.fd     = h_sd;
    new_pollfd.events = (POLLIN | POLLERR | POLLHUP);
    // meta
    p_new_meta = malloc(sizeof(rptftp_conn_meta_t));
    if (!p_new_meta)
    {
        res = RPLIB_ERROR;
        goto leave;
    }
    *p_new_meta             = (rptftp_conn_meta_t)RPTFTP_META_STATIC_INITIALIZER;
    p_new_meta->h_sd        = h_sd;
    p_new_meta->p_dirstream = p_conn_map->p_dirstream;
    p_new_meta->p_dir_path  = p_conn_map->dir_path;
    // commit
    vector_push_back(p_conn_map->vec_conn_pollfd, &new_pollfd);
    queue_ll_enqueue(p_conn_map->list_conn_meta, p_new_meta);
    // update size
    p_conn_map->size += 1;
    res = RPLIB_SUCCESS;
leave:
    return res;
}

/**
 * Remove a given connection from conn_map, cleaning up memory allocations
 * @param p_conn_map
 * @param conn_index
 * @return 0 on success, 1 on failure
 */
int
rptftp_conn_map_close_conn(rptftp_conn_map_t  *p_conn_map,
                    rptftp_conn_meta_t  *p_conn_meta,
                    ulong                conn_index)
{
    struct queue_ll_queue *p_list_conn_meta = p_conn_map->list_conn_meta;
    int                    res              = 0;

    assert(conn_index < p_conn_map->size);
    assert(p_conn_map);
    assert(p_conn_meta);

    // grab mutex
    pthread_mutex_lock(&p_conn_meta->usage_mut);
    // check
    if (0 < p_conn_meta->pending_tasks)
    {
        return RPLIB_UNSUCCESS;
    }

    pthread_mutex_unlock(&p_conn_meta->usage_mut);

    // close items
    // remove meta object from list
    // This is safe because it is a linked list and no pending tasks
    queue_ll_remove(p_list_conn_meta, conn_index, (void (*)(void *))rptftp_close_meta);
    res += vector_remove(p_conn_map->vec_conn_pollfd, conn_index);
    return (res == RPLIB_SUCCESS) ? res : RPLIB_UNSUCCESS;
}
