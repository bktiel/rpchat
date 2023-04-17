/** @file networking.h
 *
 * @brief Contains networking functionality for project
 *
 * @par
 * COPYRIGHT NOTICE: None
 */

#ifndef RPCHAT_NETWORKING_H
#define RPCHAT_NETWORKING_H

#include <netinet/in.h>

#include "conn_tracker.h"

#define RPCHAT_POLL_TIMEOUT 1000
#define RPCHAT_EPH_PORT_MIN 1024
#define RPCHAT_EPH_PORT_MAX 65535

/**
 * Helper function that sets passed socket to be re-use on address and port
 * @param h_sd Int socket descriptor
 * @return 0 on success, 1 on failure
 */
int rpchat_set_reuse_socket(int h_sd);

/**
 * Creates a UDP server on the specified port
 * @param port Port to bind UDP server to
 * @return Created socket, or -1 on failure/error
 */
int rpchat_create_new_udp_socket(int port);

/**
 * Monitors array of sockets contained in passed conn_map.
 * On activity, determines packet type and validates against that connection
 * before passing to work queue
 * @param p_conn_map Pointer to conn_map object
 * @return 0 on success, 1 on failure/error
 */
int rpchat_monitor_sockets(rpchat_conn_tracker_t *p_conn_map);

/**
 * Accepts a connection from the server socket: receives packet from source and
 * creates new conn_meta object in the connection_tracker
 * @param p_conn_map Pointer to connection object
 * @param p_server_meta Pointer to server (root) connection_meta object
 * @return 0 on success, 1 on problems encountered
 */
int rpchat_spawn_new_conn(rpchat_conn_tracker_t  *p_conn_map,
                          rpchat_conn_meta_t  *p_server_meta);

/**
 * Receives data of size TFTP_MKT_MAX on the socket descriptor described by
 * passed conn_meta object, storing it in the p_pkt_buf field.
 * @param p_conn_meta Pointer to tftp_conn_meta object
 * @param p_buf Pointer to buffer on which to store received data
 * @param client_addr Optional parameter to store remote address information
 * @return bytes read on receive success, -1 on failure
 */
int rpchat_recv_packet(rpchat_conn_meta_t *p_conn_meta,
                       char               *p_buf,
                       struct sockaddr_in *p_client_addr);

/**
 * Sends data contained in passed buffer to the socket descriptor described by
 * passed conn_meta object, up to (len) size.
 * @param p_conn_meta Pointer to conn_meta object
 * @param p_buf Pointer to buffer to send
 * @param len Length of data to send
 * @return Bytes sent on send success, 1 on failure
 */
int rpchat_send_packet(rpchat_conn_meta_t *p_conn_meta,
                       char               *p_buf,
                       size_t              len);

/**
 * Handles a Read/Write request packet.\n
 * Sanitizes packet path, then verifies packet path exists and can be
 * read/written
 * @param p_conn_meta pointer to tftp_conn_meta object to action. Must contain
 * valid packet buffer
 * @return 0 on success, 1 on failure
 */
int rpchat_handle_pkt_req(rpchat_conn_meta_t *p_conn_meta);

/**
 * Handles a Data packet.
 * Verifies path is still valid on conn_meta object and appends
 * data to that path on disk
 * @param p_conn_meta pointer to tftp_conn_meta object to action. Must contain
 * valid packet buffer
 * @return 0 on success, 1 on problems
 */
int rpchat_handle_pkt_data(rpchat_conn_meta_t *p_conn_meta);

/**
 * Handle an ack packet; prepares new data packet
 * @param p_conn_meta pointer to tftp_conn_meta object to action
 * @return 0 on success, 1 on problems
 */
int rpchat_handle_pkt_ack(rpchat_conn_meta_t *p_conn_meta);

///**
// * Handles error packet; closes connection
// * @param p_conn_meta pointer to tftp_conn_meta object to action
// * @return 0 on success, 1 on problems
// */
//int rpchat_handle_pkt_err(rpchat_conn_meta_t *p_conn_meta);

#endif // RPCHAT_NETWORKING_H

/*** end of file ***/
