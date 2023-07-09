/** @file basic_chat.h
 *
 * @brief Implements definitions specific to provided basic chat protocol
 *
 * @par
 * COPYRIGHT NOTICE: None
 */

#ifndef RPCHAT_BASIC_CHAT_H
#define RPCHAT_BASIC_CHAT_H

#include <malloc.h>
#include <stdbool.h>
#include <string.h>
#include <sys/epoll.h>

#include "components/rpchat_conn_info.h"
#include "components/rpchat_conn_queue.h"
#include "components/rpchat_string.h"
#include "rpchat_networking.h"
#include "rpchat_process_event.h"
#include "rplib_common.h"
#include "rplib_ll_queue.h"
#include "rplib_tpool.h"

#define RPCHAT_DEFAULT_PORT 9001
#define RPCHAT_DEFAULT_LOG  'stdout'
#define RPCHAT_NUM_THREADS  4

/**
 * Task-related definitions
 */

/**
 * Begin BCP server on port number with up to max_connections
 * @return RPLIB_ERROR on error, RPLIB_UNSUCCESS on problems, RPLIB_SUCCESS on
 * success
 */
int rpchat_begin_chat_server(unsigned int port_num,
                             unsigned int max_connections);

/**
 * Given activity reported by epoll on a buffer of events, take appropriate
 * action.
 * @return RPLIB_SUCCESS on no problems, RPLIB_UNSUCCESS on scheduled stop,
 * RPLIB error otherwise
 */
int rpchat_handle_events(struct epoll_event  *p_ret_event_buf,
                         int                  h_fd_server,
                         int                  h_fd_epoll,
                         int                  h_fd_signal,
                         rplib_tpool_t       *p_tpool,
                         size_t               sz_ret_event_buf,
                         rpchat_conn_queue_t *p_conn_queue);

/**
 * Handle a new incoming connection
 * @param h_fd_server
 * @return RPLIB_SUCCESS on success, RPLIB_UNSUCCESS otherwise
 */
int rpchat_handle_new_connection(unsigned int         h_fd_server,
                                 unsigned int         h_fd_epoll,
                                 rpchat_conn_queue_t *p_conn_queue);

/**
 * Handle an existing connection sending data
 * @return RPLIB_SUCCESS on success, otherwise RPLIB_UNSUCCESS
 */
int rpchat_handle_existing_connection(unsigned int         h_fd_server,
                                      unsigned int         h_fd_epoll,
                                      rpchat_conn_queue_t *p_conn_queue,
                                      struct epoll_event  *p_new_event);

/**
 * Helper function to handle signal raised and caught by epoll
 * @param h_fd_signal Signal FD signal was raised
 * @param h_fd_epoll File descriptor of epoll instance
 * @param p_tpool Pointer to threadpool handling tasks
 * @param p_conn_queue Pointer to connnection queue
 * @return RPLIB_SUCCESS if handled, RPLIB_UNSUCCESS if handled and must exit,
 * RPLIB_ERR on erroneous behavior
 */
int rpchat_handle_signal(int                  h_fd_epoll,
                         int                  h_fd_signal,
                         rplib_tpool_t       *p_tpool,
                         rpchat_conn_queue_t *p_conn_queue);

#endif // RPCHAT_BASIC_CHAT_H

/*** end of file ***/
