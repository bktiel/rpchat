/** @file rpchat_conn_info.h
 *
 * @brief Connection Queue for rpchat, including definitions for individual
 * objects
 *
 * @par
 * COPYRIGHT NOTICE: None
 */

#ifndef RPCHAT_RPCHAT_CONN_QUEUE_H
#define RPCHAT_RPCHAT_CONN_QUEUE_H

#include <assert.h>

#include "rpchat_basic_chat_util.h"
#include "rpchat_conn_info.h"
#include "rplib_ll_queue.h"
#include "rplib_tpool.h"

#define RPCHAT_SERVER_IDENTIFIER "[Server]" // used for server message prefix

/**
 * Conn_Queue holds a linked list of all conn_info objects, a mutex for it, as
 * well as globals for the lifetime of the BCP server session
 */
typedef struct
{
    rplib_ll_queue_t *p_conn_ll;     // linked list of conn_info objects
    pthread_mutex_t   mutex_conn_ll; // mutex for linked list
    int               h_fd_epoll;    // File descriptor of epoll server
    rpchat_string_t   server_str;    // String that server will use in messages
} rpchat_conn_queue_t;
/**
 * Create a rpchat_conn_queue object
 * @return Pointer to object in heap on success, NULL on failure
 */
rpchat_conn_queue_t *rpchat_conn_queue_create(int h_fd_epoll);
/**
 * Destroy an rpchat_conn_queue object
 * @return RPLIB_SUCCESS on success, RPLIB_UNSUCCESS on failure
 */
int rpchat_conn_queue_destroy(rpchat_conn_queue_t *p_conn_queue);
/**
 * Clean up a disconnected client's underlying data structures and post a
 * message to all connected clients\n\n
 * WARNING: Assumes the caller has locked the passed conn_info object's mutex.
 * Unexpected behavior will occur if it has not.
 * @param p_conn_queue Pointer to connection queue object
 * @param p_conn_info Pointer to connection info object corresponding to target
 * @return RPLIB_SUCCESS on success; RPLIB_UNSUCCESS on error
 */
int rpchat_conn_queue_destroy_conn_info(rpchat_conn_queue_t *p_conn_queue,
                                        rpchat_conn_info_t  *p_conn_info);

/**
 * Search a Queue of `rpchat_conn_info_t` objects for an object associated with
 * a given p_tgt_username
 * @param p_conn_queue Pointer to Queue of `rpchat_conn_info_t` objects
 * @param p_tgt_username Username for comparison
 * @return `rpchat_conn_info_t` associated with p_tgt_username if found;
 * otherwise NULL
 */
rpchat_conn_info_t *rpchat_conn_queue_find_by_username(
    rpchat_conn_queue_t *p_conn_queue, rpchat_string_t *p_tgt_username);

/**
 * Helper function to get all names of all clients currently connected
 * @param p_conn_queue Pointer to connection queue
 * @param p_output_buf Pointer to string to store usernames in
 * @return RPLIB_SUCCESS on success, RPLIB_UNSUCCESS otherwise
 */
int rpchat_conn_queue_list_users(rpchat_conn_queue_t *p_conn_queue,
                                 rpchat_string_t     *p_output_buf);

#endif // RPCHAT_RPCHAT_CONN_QUEUE_H

/*** end of file ***/
