/** @file rpchat_process_event.h
 *
 * @brief Definitions for event processing for basic chat protocol
 * implementation
 *
 * @par
 * COPYRIGHT NOTICE: None
 */

#ifndef RPCHAT_RPCHAT_PROCESS_EVENT_H
#define RPCHAT_RPCHAT_PROCESS_EVENT_H

#include <assert.h>
#include <malloc.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>

#include "components/rpchat_conn_info.h"
#include "components/rpchat_conn_queue.h"
#include "components/rpchat_string.h"
#include "endian.h"
#include "rpchat_basic_chat_util.h"
#include "rpchat_networking.h"
#include "rplib_common.h"
#include "rplib_tpool.h"

typedef enum
{
    RPCHAT_PROC_EVENT_INBOUND, // event is INBOUND to server (status, send, etc)
    RPCHAT_PROC_EVENT_OUTBOUND, // event is OUTBOUND to clients (e.g. deliver)
    RPCHAT_PROC_EVENT_INACTIVE  // event is explicitly to close client
} rpchat_args_proc_event_src_t;

typedef struct
{
    rpchat_args_proc_event_src_t args_type;   // Whether 'event' is inbound
    struct epoll_event           epoll_event; // Event to process
    rplib_tpool_t               *p_tpool;     // Pointer to threadpool
    rpchat_conn_info_t  *p_conn_info; // Pointer to related `rpchat_conn_info_t`
    rpchat_conn_queue_t *p_conn_queue; // Queue of `rpchat_conn_info_t`
    char                *p_msg_buf;    // Pointer to msg buffer
    size_t               sz_msg_buf;   // size of msg buffer
} rpchat_args_proc_event_t;

/**
 * Task that controls how to handle events, including state changes. Manages
 * entire lifecycle of a connection
 * \nNote: Task is re-entrant and will requeue itself to manage state
 * \nNote: Task frees passed argument pointer on completion of processing
 * @param p_args Pointer to event args
 */
void rpchat_task_conn_proc_event(void *p_args);

/**
 * Given inbound data from a client, receive appropriate headers and do
 * appropriate action based on content
 * @param p_conn_info Pointer to sender connection info
 * @param p_msg_buf Pointer to buffer containing message sent
 * @return RPLIB_SUCCESS on no issues, RPLIB_UNSUCCESS if unexpected,
 * RPLIB_ERROR if invalid msg passed
 */
int rpchat_handle_msg(rpchat_conn_queue_t           *p_conn_queue,
                      struct rpchat_connection_info *p_conn_info,
                      rplib_tpool_t                 *p_tpool,
                      char                          *p_msg_buf);
/**
 * Receive and handle a BCP RPCHAT_BCP_REGISTER message - register client and
 * send status message
 * @param p_conn_info Pointer to sender connection info
 * @param p_conn_queue Pointer to connection Queue
 * @param p_tpool Pointer to threadpool managing tasks
 * @return RPLIB_SUCCESS on no issues, RPLIB_UNSUCCESS on registration failure
 */
int rpchat_handle_register(rpchat_conn_queue_t           *p_conn_queue,
                           struct rpchat_connection_info *p_conn_info,
                           rplib_tpool_t                 *p_tpool);
/**
 * Receive and handle a BCP RPCHAT_BCP_SEND message - broadcast message to every
 * client connected (except sender)
 * @param p_conn_queue Pointer to connection Queue\
 * @param p_sender_info Pointer to sender connection info
 * @param p_tpool Pointer to threadpool managing tasks
 * @return RP_SUCCESS on no issues, RPLIB_UNSUCCESS on processing failure
 */
int rpchat_handle_send(rpchat_conn_queue_t           *p_conn_queue,
                       struct rpchat_connection_info *p_sender_info,
                       rplib_tpool_t                 *p_tpool);

/**
 * Receive and handle an BCP RPCHAT_BCP_STATUS message
 * @param p_conn_info Pointer to sender connection info
 * @return RP_SUCCESS on no issues, RP_UNSUCCESS on processing failure
 */
int rpchat_conn_info_handle_status(rpchat_conn_info_t *p_conn_info);

/**
 * Sanitize and send a message to every client connected to the BCP session
 * except for the sender identified by passed `p_sender_info` by submitting jobs
 * to threadpool
 * @param p_conn_queue Pointer to queue containing conn info objects
 * @param p_sender_info Pointer to sender connection info
 * @param p_sender_str Pointer to string containing the sender identity
 * @param p_tpool Pointer to threadpool managing tasks
 * @param p_msg Pointer to string containing message to send
 * @return RP_SUCCESS on no issues, RP_UNSUCCESS on broadcast failure
 */
int rpchat_broadcast_msg(rpchat_conn_queue_t           *p_conn_queue,
                         struct rpchat_connection_info *p_sender_info,
                         rpchat_string_t               *p_sender_str,
                         rplib_tpool_t                 *p_tpool,
                         rpchat_string_t               *p_msg);

/**
 * Send a message to a client specified by a `rpchat_conn_info_t` object
 * @param p_sender_info Pointer to `rpchat_conn_info_t` object
 * @param p_msg_buf Pointer to buffer containing valid BCP message
 * @param sz_msg_buf Size of passed message buffer
 * @return RPLIB_SUCCESS on success; otherwise, RPLIB_UNSUCCESS
 */
int rpchat_conn_info_submit_msg(rpchat_conn_info_t *p_sender_info,
                                char               *p_msg_buf,
                                size_t              sz_msg_buf);

#endif // RPCHAT_RPCHAT_PROCESS_EVENT_H
