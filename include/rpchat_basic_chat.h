/** @file basic_chat.h
 *
 * @brief Implements definitions specific to provided basic chat protocol
 *
 * @par
 * COPYRIGHT NOTICE: None
 */

#include <sys/epoll.h>

#include "rplib_ll_queue.h"
#include "rplib_tpool.h"

#ifndef RPCHAT_BASIC_CHAT_H
#define RPCHAT_BASIC_CHAT_H

#define RPCHAT_DEFAULT_PORT         9001
#define RPCHAT_DEFAULT_LOG          'stdout'
#define RPCHAT_SERVER_IDENTIFIER    "[Server]"
#define RPCHAT_MAX_STR_LENGTH       4095
#define RPCHAT_MAX_INCOMING_PKT     4111
#define RPCHAT_NUM_THREADS          4
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
    RPCHAT_CONN_PRE_REGISTER,
    RPCHAT_CONN_AVAILABLE,      // connection is available to receive data
    RPCHAT_CONN_SEND_STAT,      // send a message outbound
    RPCHAT_CONN_SEND_MSG,       // send a message outbound
    RPCHAT_CONN_PENDING_STATUS, // data sent, awaiting status response
    RPCHAT_CONN_ERR,            // error state
    RPCHAT_CONN_CLOSING,        // connection has closed
} rpchat_conn_stat_t;

typedef struct rpchat_basic_chat_string
{
    u_int16_t len;
    char      contents[RPCHAT_MAX_STR_LENGTH];
} rpchat_string_t;


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

typedef struct rpchat_connection_info
{
    int                h_fd;         // descriptor of active TCP socket
    atomic_int         pending_jobs; // jobs queued using this object
    rpchat_string_t    username;     // username picked by client
    rpchat_string_t    stat_msg;     // error/status message as applicable
    pthread_mutex_t    mutex_conn;   // lock for connection
    rpchat_conn_stat_t conn_status;  // status of connection
} rpchat_conn_info_t;

/**
 * Task-related definitions
 */

typedef enum
{
    RPCHAT_PROC_EVENT_INBOUND, // event is INBOUND to server (status, send, etc)
    RPCHAT_PROC_EVENT_OUTBOUND // event is OUTBOUND to clients (e.g. deliver)
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
 * Task to handle an event on a connection based on state and message content
 * @param p_args Pointer to `rpchat_args_proc_event_t` object
 * @return Requeues self until successful or error occurs
 */
void rpchat_task_conn_proc_event(void *p_args);

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
                             unsigned int max_connections,
                             char        *p_log_location);

/**
 * Search a Queue of `rpchat_conn_info_t` objects for an object associated with
 * a given p_tgt_username
 * @param p_conn_queue Pointer to Queue of `rpchat_conn_info_t` objects
 * @param p_tgt_username Username for comparison
 * @return `rpchat_conn_info_t` associated with p_tgt_username if found;
 * otherwise NULL
 */
rpchat_conn_info_t *rpchat_find_by_username(
    rpchat_conn_queue_t *rpchat_conn_queue_t, rpchat_string_t *p_tgt_username);

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
 * Clean up a disconnected client's underlying data structures and post a
 * message to all connected clients
 * @param p_conn_queue Pointer to connection queue object
 * @param p_conn_info Pointer to connection info object corresponding to target
 * @return RPLIB_SUCCESS on success; RPLIB_UNSUCCESS on error
 */
int rpchat_conn_info_destroy(rpchat_conn_info_t  *p_conn_info,
                             rpchat_conn_queue_t *p_conn_queue,
                             rplib_tpool_t       *p_tpool);

/**
 * Assesses a complete msg buffer and returns the type of transaction
 * @param buf
 * @return Appropriate packet type, or RPLIB_UNSUCCESS on failure
 */
rpchat_msg_type_t rpchat_get_msg_type(char *p_msg_buf);

/**
 * Given a message from a client, do appropriate action based on content
 * @param p_conn_info Pointer to sender connection info
 * @param p_msg_buf Pointer to buffer containing message sent
 * @return RPLIB_SUCCESS on no issues, RPLIB_UNSUCCESS if unexpected,
 * RPLIB_ERROR if invalid msg passed
 */
int rpchat_handle_msg(rpchat_conn_queue_t *p_conn_queue,
                      rplib_tpool_t       *p_tpool,
                      rpchat_conn_info_t  *p_conn_info,
                      char                *p_msg_buf);

/**
 * Handle a BCP RPCHAT_BCP_REGISTER message - register client and send status
 * message
 * @param p_conn_info Pointer to sender connection info
 * @param p_msg_buf Pointer to buffer containing message sent
 * @return RPLIB_SUCCESS on no issues, RPLIB_UNSUCCESS on registration failure
 */
int rpchat_handle_register(rpchat_conn_queue_t *p_conn_queue,
                           rplib_tpool_t       *p_tpool,
                           rpchat_conn_info_t  *p_conn_info,
                           char                *p_msg_buf);

/**
 * Handle a BCP RPCHAT_BCP_SEND message - broadcast message to every client
 * connected (except sender)
 * @param p_sender_info Pointer to sender connection info
 * @param p_msg_buf Pointer to buffer containing message sent
 * @return RP_SUCCESS on no issues, RPLIB_UNSUCCESS on processing failure
 */
int rpchat_handle_send(rpchat_conn_queue_t *p_conn_queue,
                       rplib_tpool_t       *p_tpool,
                       rpchat_conn_info_t  *p_sender_info,
                       char                *p_msg_buf);

/**
 * Handle a BCP RPCHAT_BCP_STATUS message
 * @param p_conn_info Pointer to sender connection info
 * @param p_msg_buf Pointer to buffer containing message sent
 * @return RP_SUCCESS on no issues, RP_UNSUCCESS on processing failure
 */
int rpchat_handle_status(rpchat_conn_info_t *p_conn_info, char *p_msg_buf);

/**
 * Send a message to a client specified by a `rpchat_conn_info_t` object
 * @param p_sender_info Pointer to `rpchat_conn_info_t` object
 * @param p_msg_buf Pointer to buffer containing valid BCP message
 * @param sz_msg_buf Size of passed message buffer
 * @return RPLIB_SUCCESS on success; otherwise, RPLIB_UNSUCCESS
 */
int rpchat_submit_msg(rpchat_conn_info_t *p_sender_info,
                      char               *p_msg_buf,
                      size_t              sz_msg_buf);

/**
 * Sanitize and send a message to every client connected to the BCP session
 * except for the sender identified by passed `p_sender_info` by submitting jobs
 * to threadpool
 * @param p_sender_info Pointer to sender connection info
 * @param p_msg Pointer to buffer containing message sent
 * @return RP_SUCCESS on no issues, RP_UNSUCCESS on broadcast failure
 */
int rpchat_broadcast_msg(rpchat_conn_info_t  *p_sender_info,
                         rpchat_string_t     *p_sender_str,
                         rpchat_conn_queue_t *p_conn_queue,
                         rplib_tpool_t       *p_tpool,
                         rpchat_string_t     *p_msg);

/**
 * Sanitize a string to only printable characters
 * @param p_input_string Pointer to input string
 * @param p_output_string Pointer to string to store output
 * @param b_allow_ctrl If true, allow control characters like \n,\t,\w;
 * otherwise, only match printable ascii (excl. space)
 * @return RPLIB_SUCCESS if no issues; otherwise, RPLIB_UNSUCCESS
 */
int rpchat_string_sanitize(rpchat_string_t *p_input_string,
                           rpchat_string_t *p_output_string,
                           bool             b_allow_ctrl);

#endif // RPCHAT_BASIC_CHAT_H

/*** end of file ***/
