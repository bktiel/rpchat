/** @file basic_chat.h
 *
 * @brief Implements definitions specific to provided basic chat protocol
 *
 * @par
 * COPYRIGHT NOTICE: None
 */

#ifndef RPCHAT_BASIC_CHAT_H
#define RPCHAT_BASIC_CHAT_H

#define RPCHAT_DEFAULT_PORT   9001
#define RPCHAT_DEFAULT_LOG    'stdout'
#define RPCHAT_MAX_STR_LENGTH 4095

typedef enum
{
    REGISTER = 1,
    SEND     = 2,
    DELIVER  = 3,
    STATUS   = 4,
} rpchat_msg_type;

typedef struct rpchat_basic_chat_string
{
    u_int16_t len;
    char      contents[RPCHAT_MAX_STR_LENGTH];
} rpchat_string_t;

/**
 * Assesses a complete msg buffer and returns the type of transaction
 * @param buf
 * @return Appropriate packet type, or NULL on failure
 */
rpchat_msg_type rpchat_get_msg_type(char *buf);

/**
 * Given a message from a client, do appropriate action based on content
 * @param sender_fd
 * @param buf
 * @return
 */
int rpchat_handle_msg(int sender_fd, char *buf);

int rpchat_handle_register(char *buf);

int rpchat_handle_send(char *buf);

int rpchat_send_deliver(char *buf);

int rpchat_send_status(u_int8_t status);



#endif // RPCHAT_BASIC_CHAT_H

/*** end of file ***/
