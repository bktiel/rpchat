/** @file basic_chat.h
 *
 * @brief Implements definitions specific to provided basic chat protocol
 *
 * @par
 * COPYRIGHT NOTICE: None
 */

#include <sys/epoll.h>

#include "rplib_ll_queue.h"

#ifndef RPCHAT_BASIC_CHAT_H
#define RPCHAT_BASIC_CHAT_H

#define RPCHAT_DEFAULT_PORT         9001
#define RPCHAT_DEFAULT_LOG          'stdout'
#define RPCHAT_MAX_STR_LENGTH       4095
#define RPCHAT_MAX_INCOMING_PKT     4111
#define RPCHAT_NUM_THREADS 4
#define RPCHAT_FILTER_ASCII_START   33
#define RPCHAT_FILTER_ASCII_SPACE   32
#define RPCHAT_FILTER_ASCII_NEWLINE 10
#define RPCHAT_FILTER_ASCII_TAB     9
#define RPCHAT_FILTER_ASCII_END     126

typedef enum rpchat_message_type
{
    RPCHAT_BCP_REGISTER = 1,
    RPCHAT_BCP_SEND     = 2,
    RPCHAT_BCP_DELIVER  = 3,
    RPCHAT_BCP_STATUS   = 4,
} rpchat_msg_type_t;

typedef enum rpchat_bcp_status_code
{
    RPCHAT_BCP_STATUS_GOOD  = 0,
    RPCHAT_BCP_STATUS_ERROR = 1,
} rpchat_stat_code_t;

typedef enum rpchat_connection_status
{
    RPCHAT_CONN_AVAILABLE,       // connection is available for sending data
    RPCHAT_CONN_PENDING_DELIVER, // connection has data to send
    RPCHAT_CONN_PENDING_STATUS,  // data sent, awaiting status response
    RPCHAT_CONN_BAD,             // error state
} rpchat_conn_stat_t;

typedef struct rpchat_basic_chat_string
{
    u_int16_t len;
    char      contents[RPCHAT_MAX_STR_LENGTH];
} rpchat_string_t;

typedef struct rpchat_connection_info
{
    int                h_fd;        // descriptor of active TCP socket
    rpchat_string_t    username;    // username picked by client
    rpchat_string_t    err_msg;     // error message as applicable
    rpchat_conn_stat_t conn_status; // status of connection
} rpchat_conn_info_t;

/**
 * Message definitions
 */

typedef struct __attribute__((__packed__)) rpchat_packet_register
{
    uint8_t         opcode;   // BCP message type
    rpchat_string_t username; // client username
} rpchat_pkt_register_t;

typedef struct __attribute__((__packed__)) rpchat_packet_send
{
    uint8_t         opcode;  // BCP message type
    rpchat_string_t message; // Message from client
} rpchat_pkt_send_t;

typedef struct __attribute__((__packed__)) rpchat_packet_deliver
{
    uint8_t         opcode;  // BCP message type
    rpchat_string_t from;    // Client message sent from
    rpchat_string_t message; // Message from client
} rpchat_pkt_deliver_t;

typedef struct __attribute__((__packed__)) rpchat_packet_status
{
    uint8_t         opcode;  // BCP message type
    uint8_t         code;    // Status code
    rpchat_string_t message; // Status message (as applicable)
} rpchat_pkt_status_t;

/**
 * Begin BCP server on port number with up to max_connections
 * @return RPLIB_ERROR on error, RPLIB_UNSUCCESS on problems, RPLIB_SUCCESS on
 * success
 */
int rpchat_begin_chat_server(unsigned int port_num,
                             unsigned int max_connections);


/**
 * Search a Queue of `rpchat_conn_info_t` objects for an object associated with
 * a given p_tgt_username
 * @param p_conn_queue Pointer to Queue of `rpchat_conn_info_t` objects
 * @param p_tgt_username Username for comparison
 * @return `rpchat_conn_info_t` associated with p_tgt_username if found;
 * otherwise NULL
 */
rpchat_conn_info_t *
rpchat_find_by_username(rplib_ll_queue_t *p_conn_queue,
                        rpchat_string_t  *p_tgt_username);

/**
 * Given activity reported by epoll on a buffer of events, take appropriate
 * action.
 * @return RPLIB_SUCCESS on no problems, otherwise, RP_UNSUCCESS
 */
int rpchat_handle_events(struct epoll_event *p_ret_event_buf,
                         int                 h_fd_server,
                         int                 h_fd_epoll,
                         unsigned int        max_connections,
                         rplib_ll_queue_t   *p_conn_queue);

/**
 * Handle a new incoming connection
 * @param h_fd_server
 * @return RPLIB_SUCCESS on success, RPLIB_UNSUCCESS otherwise
 */
int rpchat_handle_new_connection(unsigned int      h_fd_server,
                                 unsigned int      h_fd_epoll,
                                 rplib_ll_queue_t *p_conn_queue);

/**
 * Handle an existing connection sending data
 * @return RPLIB_SUCCESS on success, otherwise RPLIB_UNSUCCESS
 */
int rpchat_handle_existing_connection(unsigned int        h_fd_server,
                                      unsigned int        h_fd_epoll,
                                      rplib_ll_queue_t   *p_conn_queue,
                                      struct epoll_event *p_new_event);

/**
 * Assesses a complete msg buffer and returns the type of transaction
 * @param buf
 * @return Appropriate packet type, or RPLIB_UNSUCCESS on failure
 */
rpchat_msg_type_t rpchat_get_msg_type(char *p_msg_buf);

/**
 * Given a message from a client, do appropriate action based on content
 * @param p_sender_info Pointer to sender connection info
 * @param p_msg_buf Pointer to buffer containing message sent
 * @return RP_SUCCESS on no issues, RP_UNSUCCESS on issues
 */
int rpchat_handle_msg(rplib_ll_queue_t   *p_conn_queue,
                      rpchat_conn_info_t *p_sender_info,
                      char               *p_msg_buf);

/**
 * Handle a BCP RPCHAT_BCP_REGISTER message - register client and send status
 * message
 * @param p_sender_info Pointer to sender connection info
 * @param p_msg_buf Pointer to buffer containing message sent
 * @return RP_SUCCESS on no issues, RP_UNSUCCESS on registration failure
 */
int rpchat_handle_register(rplib_ll_queue_t   *p_conn_queue,
                           rpchat_conn_info_t *p_sender_info,
                           char               *p_msg_buf);

/**
 * Handle a BCP RPCHAT_BCP_SEND message - broadcast message to every client
 * connected (except sender)
 * @param p_sender_info Pointer to sender connection info
 * @param p_msg_buf Pointer to buffer containing message sent
 * @return RP_SUCCESS on no issues, RP_UNSUCCESS on send failure
 */
int rpchat_handle_send(rplib_ll_queue_t   *p_conn_queue,
                       rpchat_conn_info_t *p_sender_info,
                       char               *p_msg_buf);

/**
 * Handle a BCP RPCHAT_BCP_STATUS message
 * @param p_sender_info Pointer to sender connection info
 * @param p_msg_buf Pointer to buffer containing message sent
 * @return RP_SUCCESS on no issues, RP_UNSUCCESS on registration failure
 */
int rpchat_handle_status(rpchat_conn_info_t *p_sender_info, char *p_msg_buf);

int rpchat_send_deliver(rpchat_conn_info_t *p_sender_info, char *p_msg_buf);

int rpchat_send_status(rpchat_conn_info_t *p_sender_info, u_int8_t status);

#endif // RPCHAT_BASIC_CHAT_H

/*** end of file ***/
