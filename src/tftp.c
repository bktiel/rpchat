/** @file tftp.c
 *
 * @brief Entry point for program. Handles file input and makes necessary
 * library calls to implement TFTP server (RFC 1350)
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2018 Barr Group. All rights reserved.
 */

#include "tftp.h"

#include <endian.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include "io.h"
#include "networking.h"
#include "rp_common.h"

int
rptftp_conn_meta_t_set_err(rptftp_conn_meta_t *p_conn_meta,
                         tftp_error_t      err_code,
                         const char       *p_err_msg)
{
    p_conn_meta->conn_state = RPTFTP_CONN_ERROR;
    p_conn_meta->error_code = err_code;
    strncpy(p_conn_meta->error_msg, p_err_msg, RPTFTP_MAX_ERR_LEN);
    return RPLIB_SUCCESS;
}

int
rptftp_check_packet_type(char *p_packet_buf)
{
    int      res = 0;
    uint16_t opcode;
    // get first 2 bytes
    memcpy(&opcode, p_packet_buf, RPTFTP_OPCODE_LEN);
    if (!RPLIB_IS_BIG_ENDIAN)
    {
        opcode = be16toh(opcode);
    }
    switch (opcode)
    {
        case RPTFTP_PKT_ACKNOWLEDGEMENT:
            res = RPTFTP_PKT_ACKNOWLEDGEMENT;
            break;
        case RPTFTP_PKT_ERROR:
            res = RPTFTP_PKT_ERROR;
            break;
        case RPTFTP_PKT_DATA:
            res = RPTFTP_PKT_DATA;
            break;
        case RPTFTP_PKT_READ_REQUEST:
            res = RPTFTP_PKT_READ_REQUEST;
            break;
        case RPTFTP_PKT_WRITE_REQUEST:
            res = RPTFTP_PKT_WRITE_REQUEST;
            break;
        default:
            res = RPLIB_ERROR;
    }

    return res;
}

int
rptftp_get_tftp_error(rptftp_conn_meta_t *p_conn_meta)
{
    // failure
    switch (errno)
    {
        case EACCES:
            rptftp_conn_meta_t_set_err(
                p_conn_meta, RPTFTP_ERR_ACCESS_VIOLATION, "Access violation.");
            break;
        case EPERM:
            rptftp_conn_meta_t_set_err(
                p_conn_meta, RPTFTP_ERR_ACCESS_VIOLATION, "Access violation.");
            break;
        case EDQUOT:
            rptftp_conn_meta_t_set_err(p_conn_meta,
                                       RPTFTP_ERR_DISK_FULL,
                                       "Disk full or allocation exceeded.");
            break;
        case ENOSPC:
            rptftp_conn_meta_t_set_err(p_conn_meta,
                                       RPTFTP_ERR_DISK_FULL,
                                       "Disk full or allocation exceeded.");
            break;
        case EEXIST:
            rptftp_conn_meta_t_set_err(
                p_conn_meta, RPTFTP_ERR_FILE_EXISTS, "File already exists");
            break;
        case EISDIR:
            rptftp_conn_meta_t_set_err(p_conn_meta,
                                       RPTFTP_ERR_ILLEGAL_FTP_TRANSACTION,
                                       "File is a directory.");
            break;
        case ENOENT:
            rptftp_conn_meta_t_set_err(
                p_conn_meta, RPTFTP_ERR_FILE_NOT_FOUND, "File not found.");
            break;
        default:
            rptftp_conn_meta_t_set_err(
                p_conn_meta, RPTFTP_ERR_NOT_DEFINED, "Internal server error");
            break;
    }
    return RPLIB_SUCCESS;
}

int
rptftp_gen_ack_pkt(rptftp_conn_meta_t *p_conn_meta)
{
    int               res = 0;
    rptftp_ack_header_t ack_pkt;
    ack_pkt.opcode    = RPTFTP_PKT_ACKNOWLEDGEMENT;
    ack_pkt.block_num = p_conn_meta->last_block_num;
    // endianness
    if (!RPLIB_IS_BIG_ENDIAN)
    {
        ack_pkt.block_num = htobe16(ack_pkt.block_num);
        ack_pkt.opcode    = htobe16(ack_pkt.opcode);
    }
    // copy into meta object
    memset(p_conn_meta->p_pkt_buf, 0, RPTFTP_PKT_MAX);
    memcpy(p_conn_meta->p_pkt_buf, &ack_pkt, sizeof(ack_pkt));
    p_conn_meta->len_pkt_buf = sizeof(ack_pkt);
    return RPLIB_SUCCESS;
}

int
rptftp_gen_data_pkt(rptftp_conn_meta_t *p_conn_meta)
{
    rptftp_data_header_t data_pkt;
    int                h_file_fd  = -1;
    int                bytes_read = -1;
    int                pkt_len    = 0;
    int                res        = RPLIB_UNSUCCESS;

    // set fields
    data_pkt.opcode    = RPTFTP_PKT_DATA;
    data_pkt.block_num = ++p_conn_meta->last_block_num;
    if (!RPLIB_IS_BIG_ENDIAN)
    {
        data_pkt.opcode    = htobe16(data_pkt.opcode);
        data_pkt.block_num = htobe16(data_pkt.block_num);
    }
    // attempt to open file
    h_file_fd = rplib_get_file(p_conn_meta->p_dirstream,
                               p_conn_meta->file_name,
                               O_RDONLY,
                               S_IRUSR,
                               p_conn_meta->p_dir_path);
    if (0 > h_file_fd)
    {
        rptftp_get_tftp_error(p_conn_meta);
        res = RPLIB_ERROR;
        goto leave;
    }
    // attempt to read bytes from file
    bytes_read = rplib_read_data(
        h_file_fd, p_conn_meta->bytes_read, data_pkt.data, RPTFTP_DATA_MAX);
    if (0 > bytes_read)
    {
        res = RPLIB_ERROR;
        goto leave;
    }
    pkt_len = ((sizeof(data_pkt) - RPTFTP_DATA_MAX) + bytes_read);
    // place packet in meta object
    if (!memset(p_conn_meta->p_pkt_buf, 0, RPTFTP_PKT_MAX))
    {
        res = RPLIB_UNSUCCESS;
        goto leave;
    }
    if (!memcpy(p_conn_meta->p_pkt_buf, &data_pkt, pkt_len))
    {
        res = RPLIB_UNSUCCESS;
        goto leave;
    }
    // update fields
    p_conn_meta->len_pkt_buf = pkt_len;
    p_conn_meta->bytes_read += bytes_read;
    res = bytes_read;
leave:
    rplib_close_file(h_file_fd);
    return res;
}

int
rptftp_gen_err_pkt(rptftp_conn_meta_t *p_conn_meta)
{
    int               res = RPLIB_UNSUCCESS;
    rptftp_err_header_t err_pkt;
    int               err_len = 0;
    int               pkt_len = 0;
    // copy fields over
    err_pkt.opcode   = RPTFTP_PKT_ERROR;
    err_pkt.err_code = p_conn_meta->error_code;
    // endianness
    if (!RPLIB_IS_BIG_ENDIAN)
    {
        err_pkt.opcode   = htobe16(err_pkt.opcode);
        err_pkt.err_code = htobe16(err_pkt.err_code);
    }
    // copy over message
    err_len = snprintf(
        err_pkt.err_msg, RPTFTP_MAX_ERR_LEN, "%s", p_conn_meta->error_msg);
    // place packet in meta object
    pkt_len = (sizeof(err_pkt) - RPTFTP_MAX_ERR_LEN) + err_len;
    if (!memset(p_conn_meta->p_pkt_buf, 0, RPTFTP_PKT_MAX))
    {
        res = RPLIB_UNSUCCESS;
        goto leave;
    }
    if (!memcpy(p_conn_meta->p_pkt_buf, &err_pkt, pkt_len))
    {
        res = RPLIB_UNSUCCESS;
        goto leave;
    }
    // update fields
    p_conn_meta->len_pkt_buf = pkt_len;
    res                      = RPLIB_SUCCESS;
leave:
    return res;
}