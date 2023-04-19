/** @file networking.h
 *
 * @brief Implements networking components required for rpchat program
 *
 * @par
 * COPYRIGHT NOTICE: None
 */

#ifndef RPCHAT_NETWORKING_H
#define RPCHAT_NETWORKING_H

/**
 * Begin basic chat server with given arguments
 * @param port_num Port number to serve on
 * @param max_connections Maximum concurrent connections
 * @return RPLIB_SUCCESS on success, RPLIB_UNSUCCESS on failure
 */
int rpchat_begin_networking(unsigned int port_num,
                            unsigned int max_connections);

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
int rpchat_monitor_connections(int h_fd_epoll, int max_connections);

/**
 * Receive message from a given client
 * @param client_fd FD of client to receive message from
 * @param p_buf Pointer to buffer in which to place message
 * @return RPLIB_SUCCESS on success, else RPLIB_UNSUCCESS
 */
int rpchat_receive_pkt(int client_fd, char *p_buf);

/**
 * Send a message to a given client
 * @param client_fd FD of client to send message to
 * @param p_buf Pointer to buffer containing message to send
 * @return RPLIB_SUCCESS on success, else RPLIB_UNSUCCESS
 */
int rpchat_send_pkt(int client_fd, char *p_buf);

#endif // RPCHAT_NETWORKING_H

/*** end of file ***/