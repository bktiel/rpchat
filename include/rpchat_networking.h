/** @file rpchat_networking.h
 *
 * @brief Implements networking components required for rpchat program
 *
 * @par
 * COPYRIGHT NOTICE: None
 */

#ifndef RPCHAT_NETWORKING_H
#define RPCHAT_NETWORKING_H

#include <asm-generic/errno.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/time.h>
#include <unistd.h>

#include "components/rpchat_conn_queue.h"
#include "rplib_common.h"

#define RPCHAT_DEFAULT_PORT 9001
#define RPCHAT_MAX_INCOMING_MSG      8195 // maximum expected msg size
#define RPCHAT_CLIENT_AUDIT_INTERVAL 10   // seconds to wait between checks
#define RPCHAT_CONNECTION_TIMEOUT    60   // seconds to wait before terminating

/**
 * Begin networking for basic chat server with given arguments
 * @param port_num Port number to serve on
 * @param max_connections Maximum concurrent connections
 * @return Epoll file descriptor on success, RPLIB_ERROR on failure
 */
int rpchat_begin_networking(unsigned int port_num,
                            int         *p_h_fd_server,
                            int         *p_h_fd_epoll,
                            int         *p_h_fd_signal);

/**
 * Stop networking for basic chat server
 * @param h_fd_epoll Epoll instance file descriptor
 */
void rpchat_stop_networking(int h_fd_epoll, int h_fd_server, int h_fd_signal);

/**
 * Setup a TCP server socket
 * @param port_num Port number that this server socket should live on
 * @return Server socket file descriptor, otherwise RPLIB_ERROR
 */
int rpchat_setup_server_socket(unsigned int port_num);

/**
 * Monitor events on an epoll instance indefinitely or until a termination
 * signal is received
 * @param h_fd_epoll epoll file descriptor to monitor
 * @param max_connections Maximum concurrent connections
 * @return # events if no problems, RPLIB_ERR if problems
 */
int rpchat_monitor_connections(int                 h_fd_epoll,
                               struct epoll_event *p_ret_event_buf,
                               unsigned int        max_connections);

/**
 * Accept a new connection and return the socket descriptor
 * @param h_fd_server File descriptor for server
 * @return Socket descriptor of new connection on success, RPLIB_ERROR on
 * problems
 */
int rpchat_accept_new_connection(unsigned int h_fd_server);

/**
 * Close a connection and dependencies
 * @param h_fd_epoll File descriptor for related epoll instance
 * @param h_fd File descriptor for related connection
 * @return RPLIB_SUCCESS on no problems, RPLIB_UNSUCCESS otherwise
 */
int rpchat_close_connection(int h_fd_epoll, int h_fd);

/**
 * Helper function to get a signal number raised
 * @param h_fd_signal Signal FD signal was raised
 * @return RPLIB_SUCCESS if handled, RPLIB_UNSUCCESS if handled and must exit,
 * RPLIB_ERR on erroneous behavior
 */
int rpchat_get_signal(int h_fd_signal);

/**
 * Receive message from a given client
 * @param h_fd_client FD of client to receive message from
 * @param p_buf Pointer to buffer in which to place message
 * @return Bytes read, RPLIB_ERROR on error
 */
int rpchat_recv(int h_fd_client, char *p_buf, size_t len);

/**
 * Send a message to a given client
 * @param h_fd_client FD of client to send message to
 * @param p_buf Pointer to buffer containing message to send
 * @return RPLIB_SUCCESS on success, else RPLIB_UNSUCCESS
 */
int rpchat_sendmsg(int h_fd_client, char *p_buf, size_t len);

#endif // RPCHAT_NETWORKING_H

/*** end of file ***/
