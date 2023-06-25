/** @file networking.h
 *
 * @brief Implements networking components required for rpchat program
 *
 * @par
 * COPYRIGHT NOTICE: None
 */

#include <sys/epoll.h>

#ifndef RPCHAT_NETWORKING_H
#define RPCHAT_NETWORKING_H

#define RPCHAT_MAX_TCP_MSG 4111

/**
 * Begin networking for basic chat server with given arguments
 * @param port_num Port number to serve on
 * @param max_connections Maximum concurrent connections
 * @return Epoll file descriptor on success, RPLIB_ERROR on failure
 */
int rpchat_begin_networking(unsigned int port_num,
                            int         *p_h_fd_server,
                            int         *p_h_fd_epoll);

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
 * @return RP_SUCCESS if no problems encountered, otherwise, RP_UNSUCCESS
 */
int rpchat_monitor_connections(int                 h_fd_epoll,
                               struct epoll_event *p_ret_event_buf,
                               unsigned int        max_connections);

/**
 * Accept a new connection and return the socket descriptor
 * @param h_fd_server
 * @return Socket descriptor of new connection on success, RPLIB_ERROR on problems
 */
int rpchat_accept_new_connection(unsigned int h_fd_server);

/**
 * Receive message from a given client
 * @param h_fd_client FD of client to receive message from
 * @param p_buf Pointer to buffer in which to place message
 * @return Bytes read
 */
int rpchat_recvmsg(int h_fd_client, size_t len, char *p_buf);

/**
 * Send a message to a given client
 * @param h_fd_client FD of client to send message to
 * @param p_buf Pointer to buffer containing message to send
 * @return RPLIB_SUCCESS on success, else RPLIB_UNSUCCESS
 */
int rpchat_sendmsg(int h_fd_client, size_t len, char *p_buf);

#endif // RPCHAT_NETWORKING_H

/*** end of file ***/
