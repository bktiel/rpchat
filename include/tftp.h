/** @file tftp.h
 *
 * @brief Header file containing TFTP peculiar definitions (RFC 1350)
 *
 * @par
 * COPYRIGHT NOTICE: None
 */

#ifndef RPTFTP_TFTP_H
#define RPTFTP_TFTP_H

#include <bits/pthreadtypes.h>
#include <bits/types/time_t.h>
#include <dirent.h>
#include <netinet/in.h>
#include <stddef.h>

#include "stdint.h"

#define RPTFTP_LISTEN_PORT 69
#define RPTFTP_PKT_MAX     516 // maximum size of a packet
#define RPTFTP_DATA_MAX    512 // maximum size of data inside a data packet
#define RPTFTP_PATH_MAX    504 // maximum length of a filename
#define RPTFTP_MODE_LEN    8   // maximum byte length of rrq/wrq mode ("netascii")
#define RPTFTP_OPCODE_LEN  2   // byte length of an opcode
#define RPTFTP_MAX_ERR_LEN 128 // maximum length of an error message

/**
 * Opcodes passed in TFTP packet header
 */
typedef enum
{
    RPTFTP_PKT_READ_REQUEST = 1,
    RPTFTP_PKT_WRITE_REQUEST = 2,
    RPTFTP_PKT_DATA          = 3,
    RPTFTP_PKT_ACKNOWLEDGEMENT = 4,
    RPTFTP_PKT_ERROR           = 5
} tftp_opcode_t;

/**
 * Error codes that can be returned in an error packet
 */
typedef enum
{
    RPTFTP_ERR_NOT_DEFINED           = 0,
    RPTFTP_ERR_FILE_NOT_FOUND        = 1,
    RPTFTP_ERR_ACCESS_VIOLATION      = 2,
    RPTFTP_ERR_DISK_FULL             = 3,
    RPTFTP_ERR_ILLEGAL_FTP_TRANSACTION = 4,
    RPTFTP_ERR_UNKNOWN_TRANSFER_ID     = 5,
    RPTFTP_ERR_FILE_EXISTS             = 6,
    RPTFTP_ERR_NO_SUCH_USER            = 7
} tftp_error_t;

typedef enum
{
    RPTFTP_CONN_OPEN,
    RPTFTP_CONN_AWAIT_ACK,
    RPTFTP_CONN_AWAIT_DATA,
    RPTFTP_CONN_ERROR,
    RPTFTP_CONN_CLOSING,
} tftp_conn_state_t;

/**
 * Struct used to track connection related fields (beyond descriptor)
 */
typedef struct tftp_conn_meta
{
    tftp_conn_state_t  conn_state; // current connection state
    DIR               *p_dirstream;
    char              *p_dir_path;
    pthread_mutex_t    usage_mut;
    struct sockaddr_in remote_addr;
    int                error_code;                  // error code to send back
    char               error_msg[RPTFTP_MAX_ERR_LEN]; // buffer for error message
    size_t             local_tid;                   // local port
    size_t             remote_tid;                  // remote port
    char               file_name[RPTFTP_PATH_MAX];    // file to read or write
    size_t             last_block_num; // last block number on data packet
    char   p_pkt_buf[RPTFTP_PKT_MAX];    // holds a file to send in memory
    size_t len_pkt_buf;                // used size of packet buffer
    int    h_sd;                       // connection socket descriptor
    size_t bytes_read;
    time_t tm_last_active; // time that the conn was last acted on
    short  new_events;     //
    size_t pending_tasks;  // pending tasks
    size_t buf_size;       // size of p_eq_buf
} rptftp_conn_meta_t;

#define RPTFTP_META_STATIC_INITIALIZER                                         \
    {                                                                          \
        .local_tid = 0, .remote_tid = 0, .error_code = -1, .p_dir_path = NULL, \
        .len_pkt_buf = 0, .conn_state = RPTFTP_CONN_OPEN, .h_sd = -1,                 \
        .bytes_read = 0, .p_dirstream = NULL, .last_block_num = -1,            \
        .pending_tasks = 0, .buf_size = 0, .new_events = RPTFTP_NO_EVENTS,        \
        .tm_last_active = time(NULL), .usage_mut = PTHREAD_MUTEX_INITIALIZER,  \
    }

/**
 * Function to set  error on a conn_meta object. Note - sets conn_state
 * to error, but does not handle.
 * @param p_conn_meta Pointer to conn_meta object
 * @param err_code Desired error code
 * @param p_err_msg Pointer to error message to send in error packet.
 * @return 0 on success, 1 on failure
 */
int rptftp_conn_meta_t_set_err(rptftp_conn_meta_t *p_conn_meta,
                             tftp_error_t      err_code,
                             const char       *p_err_msg);

/**
 * Header for TFTP Data packets
 */
typedef struct
{
    uint16_t opcode;
    uint16_t block_num;
    char     data[512];
} __attribute__((__packed__)) rptftp_data_header_t;

/**
 * Header for TFTP Ack packets
 */
typedef struct
{
    uint16_t opcode;
    uint16_t block_num;
} __attribute__((__packed__)) rptftp_ack_header_t;

/**
 * Header for TFTP Error packets
 */
typedef struct
{
    uint16_t opcode;
    uint16_t err_code;
    char     err_msg[RPTFTP_MAX_ERR_LEN];
    uint8_t  padding;
} __attribute__((__packed__)) rptftp_err_header_t;

/*
 * Helper methods
 */

/**
 * Checks errno and sets the appropriate error flag on a conn_meta object\n
 * Note: Should be called immediately after error state detected to avoid
 * interference on errno
 * @param p_conn_meta Pointer to conn_meta object
 * @return 0 on success, 1 on failure
 */
int rptftp_get_tftp_error(rptftp_conn_meta_t *p_conn_meta);

/**
 * Assesses passed packet for type
 * @param p_packet_buf Pointer to buffer containing a received packet
 * @return Packet opcode, or -1 on failure
 */
int rptftp_check_packet_type(char *p_packet_buf);

/*
 * Packet methods
 */

/**
 * Generate data packet based on file information in a conn_meta object
 * @param p_conn_meta Pointer to conn_meta object
 * @return Size of data on success, -1 on failure
 */
int rptftp_gen_data_pkt(rptftp_conn_meta_t *p_conn_meta);

/**
 * Generate acknowledgement packet based on connection information in a
 * conn_meta object
 * @param p_conn_meta Pointer to conn_meta object
 * @return 0 on success, 1 on problems
 */
int rptftp_gen_ack_pkt(rptftp_conn_meta_t *p_conn_meta);

/**
 * Generate error packet based on error information in a conn_meta object
 * @param p_conn_meta Pointer to conn_meta object
 * @return 0 on success, 1 on problems
 */
int rptftp_gen_err_pkt(rptftp_conn_meta_t *p_conn_meta);

#endif /* RPTFTP_TFTP_H */

/*** end of file ***/
