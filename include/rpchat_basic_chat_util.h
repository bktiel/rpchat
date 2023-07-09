/** @file rpchat_basic_chat_defs.h
 *
 * @brief Specific definitions for Basic Chat Protocol and helper functions
 *
 * @par
 * COPYRIGHT NOTICE: None
 */

#ifndef RPCHAT_RPCHAT_BASIC_CHAT_UTIL_H
#define RPCHAT_RPCHAT_BASIC_CHAT_UTIL_H

#include "components/rpchat_string.h"
#include "stdint.h"
#include <sys/epoll.h>

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
 * Assesses a complete msg buffer and returns the type of transaction
 * @param buf
 * @return Appropriate packet type, or RPLIB_UNSUCCESS on failure
 */
rpchat_msg_type_t rpchat_get_msg_type(char *p_msg_buf);

/**
 * Turn an epoll descriptor on or off, making the epoll instance either ignore
 * or pay attention to inbound data
 * @param h_fd_epoll Epoll instance file descriptor
 * @param h_toggle_fd File descriptor to toggle
 * @param p_data_ptr Pointer to data connected to epoll event
 * @param enabled If true, listen to events on this connection. If false, stop
 * listening.
 * @return RPLIB_SUCCESS on success, RPLIB_UNSUCCESS on problems
 */
int rpchat_toggle_descriptor(int   h_fd_epoll,
                             int   h_toggle_fd,
                             void *p_data_ptr,
                             bool  enabled);

#endif // RPCHAT_RPCHAT_BASIC_CHAT_UTIL_H

/*** end of file ***/
