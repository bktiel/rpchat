/** @file rpchat_conn_info.h
 *
 * @brief Connection Info object for rpchat containing all items that
 * should be tracked from connection to connection
 *
 * @par
 * COPYRIGHT NOTICE: None
 */

#ifndef RPCHAT_RPCHAT_CONN_INFO_H
#define RPCHAT_RPCHAT_CONN_INFO_H

#include <assert.h>
#include <stdatomic.h>

#include "rpchat_basic_chat_util.h"
#include "rpchat_string.h"
#include "rplib_tpool.h"

typedef enum rpchat_connection_status
{
    RPCHAT_CONN_PRE_REGISTER,
    RPCHAT_CONN_AVAILABLE,      // connection is available to receive data
    RPCHAT_CONN_SEND_STAT,      // send a message outbound
    RPCHAT_CONN_SEND_MSG,       // send a message outbound
    RPCHAT_CONN_PENDING_STATUS, // data sent, awaiting status response
    RPCHAT_CONN_ERR,            // error state
    RPCHAT_CONN_CLOSING,        // connection has closed
} rpchat_conn_stat_t;

typedef struct rpchat_connection_info
{
    int                h_fd;         // descriptor of active TCP socket
    atomic_int         pending_jobs; // # of jobs queued for this client
    rpchat_string_t    username;     // username picked by client
    rpchat_string_t    stat_msg;     // error/status message as applicable
    pthread_mutex_t    mutex_conn;   // lock for connection
    rpchat_conn_stat_t conn_status;  // status of connection
    time_t             last_active;  // time connection was last active
} rpchat_conn_info_t;

/**
 * Initialize a connection info object with default values
 * @param p_new_conn_info Pointer to new connection info object
 * @param h_new_fd File descriptor to associate with this info object
 * @return RPLIB_SUCCESS on success, RPLIB_UNSUCCESS on problems
 */
int rpchat_conn_info_initialize(rpchat_conn_info_t *p_new_conn_info,
                                int                 h_new_fd);

/**
 * Enqueue tasks into threadpool
 * @param p_conn_info Pointer to `rpchat_conn_info_t` object involved in
 * transaction
 * @param p_tpool Pointer to threadpool object
 * @param p_function Function pointer to task to execute
 * @param p_arg Pointer to args to use with function
 * @return RPLIB_SUCCESS on successful queue, RPLIB_UNSUCCESS on failure
 */
int rpchat_conn_info_enqueue_task(rpchat_conn_info_t *p_conn_info,
                                  rplib_tpool_t      *p_tpool,
                                  void (*p_function)(void *p_arg),
                                  void *p_arg);

#endif // RPCHAT_RPCHAT_CONN_INFO_H

/*** end of file ***/
