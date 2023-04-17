/** @file conn_map.h
*
* @brief Implements conn_map object
*
* @par
* COPYRIGHT NOTICE: None
*/

#ifndef RPCHAT_CONN_TRACKER_H
#define RPCHAT_CONN_TRACKER_H

#include <bits/types/time_t.h>
#include <dirent.h>
#include <stddef.h>

#include "poll.h"
#include "queue_ll_lib.h"
#include "tftp.h"
#include "vector.h"

#define RPCHAT_MAP_SERV_SOCK_INDEX \
    0 // index of the server socket within conn_map
#define RPCHAT_MAP_SIGNAL_INDEX \
    1 // index of signalfd in conn_map that is used to handle SIGINT
#define RPCHAT_NO_EVENTS (-1) // value for no new events within conn_meta

/**
 * Track where a connection is in the lifecycle
 */

/*
 * Structs
 */

typedef struct rpchat_connection_tracker
{
    size_t    timeout;
    char      dir_path[PATH_MAX];
    size_t    size;
    vector_t *vec_conn_pollfd; // vector container for socket descriptors
    struct queue_ll_queue
        *list_conn_meta; // list for conn_state objects (metadata)
} rpchat_conn_tracker_t;

/*
 * Conn_map methods
 */
int rpchat_conn_tracker_initialize(rpchat_conn_tracker_t *p_conn_map, size_t timeout);
int rpchat_conn_tracker_destroy(rpchat_conn_tracker_t *p_conn_map);
int rpchat_conn_tracker_add_conn(rpchat_conn_tracker_t *p_conn_map, int h_sd);
int rpchat_conn_tracker_close_conn(rpchat_conn_tracker_t  *p_conn_map,
                                   rpchat_conn_meta_t  *p_conn_meta,
                                   ulong                conn_index);

/*
 * Helpers
 */
void rpchat_close_pollfd(struct pollfd *p_pollfd);
void rpchat_close_meta(rpchat_conn_meta_t *p_conn_meta);

#endif // RPCHAT_CONN_TRACKER_H

/*** end of file ***/
