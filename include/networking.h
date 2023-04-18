/** @file networking.h
 *
 * @brief Implements networking components required for program
 *
 * @par
 * COPYRIGHT NOTICE: None
 */

#ifndef RPCHAT_NETWORKING_H
#define RPCHAT_NETWORKING_H

#define RPCHAT_MAX_EVENTS 32 // max events to be returned from epoll at one time

/**
 * Begin basic chat server with given arguments
 * @param port_num Port number to serve on
 * @param log_location Location to store log data in (NULL is stdout)
 * @return RPLIB_SUCCESS on success, RPLIB_UNSUCCESS on failure
 */
int rpchat_begin_networking(int port_num, char *log_location);

/**
 * Setup a TCP server socket
 * @param port_num Port number that this server socket should live on
 * @return Server socket file descriptor, otherwise RPLIB_ERROR
 */
int rpchat_setup_server_socket(int port_num);

/**
 * Monitor events on an epoll instance indefinitely or until a termination
 * signal is received
 * @param epoll_fd epoll file descriptor to monitor
 * @return RP_SUCCESS if no problems encountered, otherwise, RP_UNSUCCESS
 */
int rpchat_monitor_connections(int epoll_fd);

#endif // RPCHAT_NETWORKING_H

/*** end of file ***/