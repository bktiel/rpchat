/** @file networking.c
 *
 * @brief Contains definitions for required networking
 *
 * @par
 * COPYRIGHT NOTICE: None
 */
#include "networking.h"

#include <assert.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "io.h"
#include "rp_common.h"

/**
 * Helper to set generic error on a conn_meta object. Note - sets conn_state
 * to error, but does not handle.
 * @param p_conn_meta Pointer to conn_meta object
 * @param p_err_msg Pointer to error message to send in error packet.
 * @return 0 on success, 1 on failure
 */
static int
tftp_conn_meta_t_set_generic_err(rptftp_conn_meta_t *p_conn_meta,
                                 const char       *p_err_msg)
{
    p_conn_meta->conn_state = RPTFTP_CONN_ERROR;
    p_conn_meta->error_code = RPTFTP_ERR_NOT_DEFINED;
    strncpy(p_conn_meta->error_msg, p_err_msg, RPTFTP_MAX_ERR_LEN);
    return RPLIB_SUCCESS;
}

/**
 * Helper function to check if a connection has timed out
 * IAW CONN_TIMEOUT
 * @param p_conn_meta
 * @return 0 on no timeout, 1 on timeout
 */
static int
check_timeout(rptftp_conn_map_t *p_conn_map, rptftp_conn_meta_t *p_conn_meta)
{
    double tm_elapsed = 0;
    tm_elapsed        = difftime(time(NULL), p_conn_meta->tm_last_active);
    return (tm_elapsed > p_conn_map->timeout);
}

/**
 * Helper function - keep trying different ports
 * until a free one is found, then return socket
 * @param client_port Port of remote - used to prevent conflicts in local
 * testing
 * @param p_h_sd Pointer to memory in which to store created socket
 * @return Chosen port number, or -1 on failure
 */
static int
get_random_eph_socket(int client_port, int *p_h_sd)
{
    int res      = RPLIB_ERROR;
    int new_port = 0;
    // iterate over ephemeral port range
    for (new_port = RPTFTP_EPH_PORT_MIN; new_port < RPTFTP_EPH_PORT_MAX; new_port++)
    {
        if (client_port == new_port)
            continue;
        res = rptftp_create_new_udp_socket(new_port);
        if (RPLIB_ERROR == res)
        {
            continue;
        }
        *p_h_sd = res;
        break;
    }
    // if exhausted entire port range (???) we have a problem
    if (new_port == RPTFTP_EPH_PORT_MAX)
    {
        res = RPLIB_ERROR;
    }
    return (RPLIB_ERROR == res) ? RPLIB_ERROR : new_port;
}

/**
 * Handle incoming data on a socket.
 * Receives packet on socket described by conn_meta object,
 * verifies header against conn_meta object, passes to appropriate
 * handler
 * @param p_conn_meta Pointer to active conn_meta object
 * @return 0 on success, 1 on failure
 */
static int
handle_new_pkt(rptftp_conn_meta_t *p_conn_meta)
{
    struct sockaddr_in client_info;
    int                bytes_read = -1;
    int                res        = 0;
    int                opcode     = RPLIB_ERROR;

    // if connection is OPEN, packet is RRQ/WRQ
    if (RPTFTP_CONN_OPEN == p_conn_meta->conn_state)
    {
        rptftp_handle_pkt_req(p_conn_meta);
        goto leave;
    }
    // otherwise, receive the packet
    bytes_read = rptftp_recv_packet(p_conn_meta, p_conn_meta->p_pkt_buf, &client_info);
    // check bytes were actually read
    if (0 > bytes_read)
    {
        tftp_conn_meta_t_set_generic_err(p_conn_meta, "Internal read error");
        res = RPLIB_UNSUCCESS;
        goto leave;
    }
    // update pkt_len for use by further calls
    p_conn_meta->len_pkt_buf = bytes_read;
    // check TIDs
    if (p_conn_meta->remote_tid != ntohs(client_info.sin_port))
    {
        rptftp_conn_meta_t_set_err(p_conn_meta,
                                   RPTFTP_ERR_UNKNOWN_TRANSFER_ID,
                                   "Unknown transfer ID.");
        goto leave;
    }
    // get this packet type
    opcode = rptftp_check_packet_type(p_conn_meta->p_pkt_buf);
    // branch
    switch (opcode)
    {
        case RPTFTP_PKT_ACKNOWLEDGEMENT:
            res = rptftp_handle_pkt_ack(p_conn_meta);
            break;
        case RPTFTP_PKT_DATA:
            res = rptftp_handle_pkt_data(p_conn_meta);
            break;
//        case RPTFTP_PKT_ERROR:
//            res = rptftp_handle_pkt_err(p_conn_meta);
//            break;
        default:
            p_conn_meta->error_code = RPTFTP_ERR_ILLEGAL_FTP_TRANSACTION;
            p_conn_meta->conn_state = RPTFTP_CONN_ERROR;
    }
    // if res is success, offer connection stay of execution
    if (RPLIB_SUCCESS == res)
    {
        p_conn_meta->tm_last_active = time(NULL);
    }
leave:
    return res;
}

/**
 * Helper function to handle activity on socket that's not the root server
 * socket or the signal socket
 * @param p_conn_pollfd Pointer to related pollfd object
 * @param p_conn_meta Pointer to related conn_meta object
 * @return 0 on successful handling, 1 on problems encountered
 */
static int
handle_active_child_socket(struct pollfd    *p_conn_pollfd,
                           rptftp_conn_meta_t *p_conn_meta)
{
    int res = 0;
    if (p_conn_pollfd->revents & POLLERR)
    {
        p_conn_meta->conn_state = RPTFTP_CONN_ERROR;
        res                     = RPLIB_UNSUCCESS;
        goto leave;
    }
    if (p_conn_pollfd->revents & POLLIN || RPTFTP_CONN_OPEN == p_conn_meta->conn_state)
    {
        res = handle_new_pkt(p_conn_meta);
    }
leave:
    return res;
}

/**
 * Helper function that sends error message to the client defined by
 * conn_meta object and updates state to closing
 * @param p_conn_meta Pointer to conn_meta object
 * @return 0 on success, 1 on problems
 */
static int
handle_child_socket_err(rptftp_conn_meta_t *p_conn_meta)
{
    int res = RPLIB_UNSUCCESS;
    // create
    if (RPLIB_SUCCESS != rptftp_gen_err_pkt(p_conn_meta))
        goto leave;
    // attempt to send. if unable to send, wait for next round to retry
    if (0 > rptftp_send_packet(
            p_conn_meta, p_conn_meta->p_pkt_buf, p_conn_meta->len_pkt_buf))
    {
        goto leave;
    }
    res                     = RPLIB_SUCCESS;
    p_conn_meta->conn_state = RPTFTP_CONN_CLOSING;
leave:
    return res;
}

/**
 * Helper function to handle activity on signalfd socket
 *
 * @param p_conn_map Pointer to conn map object
 * @param p_conn_pollfd Pointer to the signalfd pollfd object
 * @return 0 on handled, 1 if sigint received, -1 on error
 */
static int
handle_active_signalfd(rptftp_conn_map_t *p_conn_map,
                       struct pollfd       *p_conn_pollfd)
{
    struct signalfd_siginfo siginfo; // used to consume signalfd
    int                     res   = 0;
    int                     index = 0;
    // get signal
    if (0 > read(p_conn_pollfd->fd, &siginfo, sizeof(siginfo)))
    {
        res = RPLIB_ERROR;
        goto leave;
    }
    // get type of signal
    // if sigint, close
    if (SIGINT == siginfo.ssi_signo)
    {
        res = RPLIB_UNSUCCESS;
        for (index = p_conn_map->size; 0 != index; index--)
        {
            if (RPLIB_SUCCESS
                != rptftp_conn_map_close_conn(
                    p_conn_map,
                    p_conn_map->list_conn_meta->p_rear->p_data,
                    (index - 1)))
            {
                fprintf(stderr, "Error occurred during connection cleanup..");
            }
        }
        // set to 1 to tell caller to shut down
        res = RPLIB_UNSUCCESS;
    }
leave:
    return res;
}

/*
 * Handlers
 */

int
rptftp_handle_pkt_req(rptftp_conn_meta_t *p_conn_meta)
{
    uint16_t opcode   = 0;
    int      res      = RPLIB_UNSUCCESS;
    char    *p_cursor = p_conn_meta->p_pkt_buf;
    int      file_len = 0;
    char     op_mode[RPTFTP_MODE_LEN];
    // get fields
    opcode = rptftp_check_packet_type(p_conn_meta->p_pkt_buf);
    // if not a request, leave
    if (RPTFTP_PKT_READ_REQUEST != opcode && RPTFTP_PKT_WRITE_REQUEST != opcode)
    {
        goto error;
    }
    p_cursor += RPTFTP_OPCODE_LEN;
    file_len = snprintf(p_conn_meta->file_name, RPTFTP_PATH_MAX, "%s", p_cursor);
    // + 1 for the separating null byte
    p_cursor += file_len + 1;
    snprintf(op_mode, RPTFTP_MODE_LEN, "%s", p_cursor);

    // validate req fields
    if (0 == file_len)
    {
        goto error;
    }
    if (0 != strcmp(op_mode, "octet"))
    {
        tftp_conn_meta_t_set_generic_err(p_conn_meta,
                                         "Only octet mode is supported.");
        goto error;
    }

    // specific handling
    if (RPTFTP_PKT_WRITE_REQUEST == opcode)
    {
        // validate filename
        errno = 0;
        res   = rplib_get_file(p_conn_meta->p_dirstream,
                             p_conn_meta->file_name,
                             (O_CREAT | O_EXCL),
                             (S_IRWXU),
                             p_conn_meta->p_dir_path);
        if (0 > res)
        {
            rptftp_get_tftp_error(p_conn_meta);
            goto error;
        }
        rplib_close_file(res);

        // generate ACK packet
        if (RPLIB_SUCCESS != rptftp_gen_ack_pkt(p_conn_meta))
        {
            goto error;
        }
        // send
        res = rptftp_send_packet(
            p_conn_meta, p_conn_meta->p_pkt_buf, p_conn_meta->len_pkt_buf);
        p_conn_meta->conn_state = RPTFTP_CONN_AWAIT_DATA;
        res                     = RPLIB_SUCCESS;
        goto leave;
    }
    else
    {
        // validate filename via gen_pkt
        errno = 0;
        // for a RRQ, generate DATA packet, set mode to
        if (0 > rptftp_gen_data_pkt(p_conn_meta))
        {
            // failure
            goto error;
        }
        // set completed data packet
        rptftp_send_packet(
            p_conn_meta, p_conn_meta->p_pkt_buf, p_conn_meta->len_pkt_buf);
        p_conn_meta->conn_state = RPTFTP_CONN_AWAIT_ACK;
        res                     = RPLIB_SUCCESS;
    }
    goto leave;
error:
    res                     = RPLIB_UNSUCCESS;
    p_conn_meta->conn_state = RPTFTP_CONN_ERROR;
leave:
    return res;
}

int
rptftp_handle_pkt_data(rptftp_conn_meta_t *p_conn_meta)
{
    int                 h_file_fd = -1;
    int                 res       = RPLIB_UNSUCCESS;
    rptftp_data_header_t *p_data_pkt
        = (rptftp_data_header_t *)p_conn_meta->p_pkt_buf;
    int data_length   = RPTFTP_DATA_MAX; // length of data in packet
    int bytes_written = 0;
    int perms         = 0;
    // endian
    if (!RPLIB_IS_BIG_ENDIAN)
    {
        p_data_pkt->block_num = be16toh(p_data_pkt->block_num);
    }
    // verify waiting for ack
    if (RPTFTP_CONN_AWAIT_DATA != p_conn_meta->conn_state)
    {
        goto leave;
    }
    // verify block_num is expected next
    if (p_data_pkt->block_num != (++p_conn_meta->last_block_num))
    {
        goto leave;
    }
    // attempt to write data to disk
    h_file_fd = rplib_get_file(p_conn_meta->p_dirstream,
                               p_conn_meta->file_name,
                               (O_WRONLY | O_CREAT | O_APPEND),
                               S_IRWXU,
                               p_conn_meta->p_dir_path);
    // if fail to get descriptor, error
    if (0 > h_file_fd)
    {
        goto error;
    }
    // get length of data
    if (RPTFTP_PKT_MAX != p_conn_meta->len_pkt_buf)
    {
        // derive data length by subtracting header field sizes
        data_length = p_conn_meta->len_pkt_buf
                      - (sizeof(rptftp_data_header_t) - RPTFTP_DATA_MAX);
    }
    // write data to file...
    bytes_written = rplib_write_data(h_file_fd, p_data_pkt->data, data_length);
    if (0 > bytes_written)
    {
        goto error;
    }
    // prep ack packet
    rptftp_gen_ack_pkt(p_conn_meta);
    // send ack packet
    rptftp_send_packet(
        p_conn_meta, p_conn_meta->p_pkt_buf, p_conn_meta->len_pkt_buf);
    res = RPLIB_SUCCESS;
    goto leave;
error:
    rptftp_get_tftp_error(p_conn_meta);
    p_conn_meta->conn_state = RPTFTP_CONN_ERROR;
    res                     = RPLIB_UNSUCCESS;
leave:
    if (0 < h_file_fd)
        rplib_close_file(h_file_fd);
    return res;
}

int
rptftp_handle_pkt_ack(rptftp_conn_meta_t *p_conn_meta)
{
    int                res       = RPLIB_UNSUCCESS;
    rptftp_ack_header_t *p_ack_pkt = (rptftp_ack_header_t *)p_conn_meta->p_pkt_buf;
    // endian
    if (!RPLIB_IS_BIG_ENDIAN)
    {
        p_ack_pkt->block_num = be16toh(p_ack_pkt->block_num);
    }
    // verify waiting for ack
    if (RPTFTP_CONN_AWAIT_ACK != p_conn_meta->conn_state)
    {
        goto leave;
    }
    // verify block_num is what was sent last
    if (p_ack_pkt->block_num != (p_conn_meta->last_block_num))
    {
        goto leave;
    }

    // ack received; craft data packet and send back (as applicable)
    res = rptftp_gen_data_pkt(p_conn_meta);
    if (0 > res)
    {
        // upon an error state, rptftp_gen_data_pkt sets specific error
        p_conn_meta->conn_state = RPTFTP_CONN_ERROR;
        goto leave;
    }
    // if nothing was read and the last block was not full, terminate
    if (0 == res
        && (p_conn_meta->bytes_read
            < (p_conn_meta->last_block_num * RPTFTP_DATA_MAX)))
    {
        p_conn_meta->conn_state = RPTFTP_CONN_CLOSING;
        goto leave;
    }
    // send completed packet to remote
    if (0 > rptftp_send_packet(
            p_conn_meta, p_conn_meta->p_pkt_buf, p_conn_meta->len_pkt_buf))
        goto error;
    res = RPLIB_SUCCESS;
    goto leave;
error:
    tftp_conn_meta_t_set_generic_err(p_conn_meta, "Internal Server Error");
    res = RPLIB_UNSUCCESS;
leave:
    return res;
}

//int
//rptftp_handle_pkt_err(rptftp_conn_meta_t *p_conn_meta)
//{
//}

int
rptftp_set_reuse_socket(int h_sd)
{
    int res    = RPLIB_SUCCESS;
    int enable = 1;
    res += setsockopt(h_sd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
    res += setsockopt(h_sd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable));

    return res == 0 ? RPLIB_SUCCESS : RPLIB_UNSUCCESS;
}

int
rptftp_spawn_new_conn(rptftp_conn_map_t  *p_conn_map,
               rptftp_conn_meta_t  *p_server_meta)
{
    int res         = -1;
    int opcode      = RPTFTP_PKT_ERROR; // opcode of incoming packet
    int h_new_sock  = 0;         // new socket descriptor created
    int chosen_port = 0;         // port h_new_sock was generated on

    rptftp_conn_meta_t *p_new_conn_meta
        = NULL;                         // used for new conn meta as applicable
    struct sockaddr_in client;          // used for incoming connections on 69
    int                client_port = 0; // client port

    assert(p_conn_map);
    assert(p_server_meta);

    // accept initial data
    if (0 > rptftp_recv_packet(p_server_meta, p_server_meta->p_pkt_buf, &client))
    {
        res = RPLIB_UNSUCCESS;
        goto leave;
    }

    // validate client sent port
    client_port = ntohs(client.sin_port);
    if (RPTFTP_EPH_PORT_MAX < client_port)
    {
        res = RPLIB_UNSUCCESS;
        goto leave;
    }

    // get random ephemeral port
    chosen_port = get_random_eph_socket(client_port, &h_new_sock);
    if (0 > h_new_sock)
    {
        res = RPLIB_UNSUCCESS;
        goto leave;
    }

    // new connection entry
    if (RPLIB_SUCCESS != rptftp_conn_map_add_conn(p_conn_map, h_new_sock))
    {
        res = RPLIB_UNSUCCESS;
        goto leave;
    }

    p_new_conn_meta
        = ((rptftp_conn_meta_t *)p_conn_map->list_conn_meta->p_rear->p_data);
    // verify that received packet is a request
    opcode = rptftp_check_packet_type(p_server_meta->p_pkt_buf);
    if (RPTFTP_PKT_READ_REQUEST == opcode || RPTFTP_PKT_WRITE_REQUEST == opcode)
    {
        // copy over packet data
        memcpy(
            p_new_conn_meta->p_pkt_buf, p_server_meta->p_pkt_buf,
               RPTFTP_PKT_MAX);
        // store local, remote ports for this connection
        p_new_conn_meta->remote_tid = client_port;
        p_new_conn_meta->local_tid  = chosen_port;
        // copy addr over for later
        memcpy(&p_new_conn_meta->remote_addr, &client, sizeof(client));
        // success
        res = RPLIB_SUCCESS;
    }
    else
    {
        p_new_conn_meta->conn_state = RPTFTP_CONN_ERROR;
        p_new_conn_meta->error_code = RPTFTP_ERR_ILLEGAL_FTP_TRANSACTION;
        res                         = RPLIB_UNSUCCESS;
    }
leave:
    return res;
}

int
rptftp_recv_packet(rptftp_conn_meta_t *p_conn_meta,
            char               *p_buf,
            struct sockaddr_in *p_client_addr)
{
    int                 bytes_read = 0;
    struct sockaddr_in  addr_client; // placeholder in case no addr
    struct sockaddr_in *p_addr
        = (NULL != p_client_addr) ? p_client_addr : &addr_client;
    uint addr_size = sizeof(addr_client);
    memset(p_conn_meta->p_pkt_buf, 0, RPTFTP_PKT_MAX);
    bytes_read = recvfrom(p_conn_meta->h_sd,
                          p_buf,
                          RPTFTP_PKT_MAX,
                          0,
                          (struct sockaddr *)p_addr,
                          &addr_size);
    return (0 > bytes_read) ? RPLIB_ERROR : bytes_read;
}

int
rptftp_send_packet(rptftp_conn_meta_t *p_conn_meta, char *p_buf, size_t len)
{
    int  bytes_sent = 0;
    uint addr_size  = sizeof(p_conn_meta->remote_addr);
    bytes_sent      = sendto(p_conn_meta->h_sd,
                        p_buf,
                        len,
                        0,
                        (struct sockaddr *)&p_conn_meta->remote_addr,
                        addr_size);
    return (0 > bytes_sent) ? RPLIB_ERROR : bytes_sent;
}

int
rptftp_create_new_udp_socket(int port)
{
    struct sockaddr_in addr_server;
    int                h_serv_sd = -1;
//    size_t             name_len  = 0;
//    size_t             addr_size = 0;
    int                res       = RPLIB_SUCCESS;

    // attempt to create udp socket
    h_serv_sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (0 > h_serv_sd)
    {
        res = RPLIB_ERROR;
        goto leave;
    }

    // set up particulars
    addr_server.sin_family      = AF_INET;
    addr_server.sin_port        = htons(port);
    addr_server.sin_addr.s_addr = htonl(INADDR_ANY);

    // set socket to re-use address/port
    if (RPLIB_SUCCESS != rptftp_set_reuse_socket(h_serv_sd))
    {
        res = RPLIB_ERROR;
        goto cleanup;
    }

    // bind
    if (0
        > bind(h_serv_sd, (struct sockaddr *)&addr_server, sizeof(addr_server)))
    {
        res = RPLIB_ERROR;
        goto cleanup;
    }
    goto leave;
cleanup:
    close(h_serv_sd);
leave:
    return (0 > res) ? RPLIB_ERROR : h_serv_sd;
}

int
rptftp_monitor_sockets(rptftp_conn_map_t *p_conn_map)
{
    int                    res            = RPLIB_SUCCESS;
    struct queue_ll_queue *list_conn_meta = p_conn_map->list_conn_meta;
    struct queue_ll_node  *p_current_node = list_conn_meta->p_front;
    struct queue_ll_node  *p_next_node    = NULL;
    rptftp_conn_meta_t    *p_conn_meta    = NULL;
    size_t         poll_index = 0; // used as counter when iterating pollfds
    struct pollfd *p_sd_buf_array
        = (struct pollfd *)vector_get_buffer(p_conn_map->vec_conn_pollfd);
    size_t nfds = vector_get_size(p_conn_map->vec_conn_pollfd);

    // monitor array
    if (0 > poll(p_sd_buf_array, nfds, RPTFTP_POLL_TIMEOUT))
    {
        perror("Error");
        res = RPLIB_ERROR;
        goto leave;
    }

    /*
     * Iterate
     * Case #0 - connection closing or error
     * Case #1 - nothing happening, check status/timeout
     * Case #2 - item on server socket (new connection)
     * Case #3 - item on signal socket has activity (signal received)
     * Case #4 - item on another socket has activity
     */
    while (NULL != p_current_node)
    {
        p_next_node = p_current_node->p_next;
        // get the object
        p_conn_meta = NULL;
        p_conn_meta = (rptftp_conn_meta_t *)p_current_node->p_data;
        if (!p_conn_meta)
        {
            // if invalid p_conn_meta is fed, error state
            res = RPLIB_ERROR;
            goto leave;
        }

        /*
         * Case 0 - connection in error state or closing
         */
        if (RPTFTP_CONN_ERROR == p_conn_meta->conn_state)
        {
            handle_child_socket_err(p_conn_meta);
        }
        if (RPTFTP_CONN_CLOSING == p_conn_meta->conn_state)
        {
            rptftp_conn_map_close_conn(p_conn_map, p_conn_meta, poll_index);
            poll_index--;
            goto loop_cleanup;
        }

        /*
         * Case 1 - no events, check timeout or resend packets
         */
        if (0 == p_sd_buf_array[poll_index].revents)
        {
            // if timed out, kill this connection
            if (poll_index > RPTFTP_MAP_SIGNAL_INDEX
                && check_timeout(p_conn_map, p_conn_meta))
            {
                if (RPLIB_SUCCESS
                    == rptftp_conn_map_close_conn(
                        p_conn_map, p_conn_meta, poll_index))
                    poll_index--;
                goto loop_cleanup;
            }
            // if not timed out, resend last packet (RFC 1350)
            rptftp_send_packet(
                p_conn_meta, p_conn_meta->p_pkt_buf, p_conn_meta->len_pkt_buf);
            goto loop_cleanup;
        }

        /*
         * Case 2 - new connection
         */
        if (RPTFTP_MAP_SERV_SOCK_INDEX == poll_index)
        {
            if (POLLERR & p_sd_buf_array[poll_index].revents)
            {
                res = RPLIB_ERROR;
                goto leave;
            }
            // spawn a new connection object based on connecitng client
            if (RPLIB_SUCCESS != rptftp_spawn_new_conn(p_conn_map, p_conn_meta))
            {
                fprintf(stderr, "Unable to create new connection object.\n");
                goto loop_cleanup;
            }
            // handle new packet
            handle_new_pkt(list_conn_meta->p_rear->p_data);
            goto loop_cleanup;
        }
        /*
         * Case 3 - signal received
         */
        if (RPTFTP_MAP_SIGNAL_INDEX == poll_index
            && p_sd_buf_array[poll_index].revents & (POLLIN))
        {
            // if handle_signalfd returns 1, close application
            if (RPLIB_UNSUCCESS
                == handle_active_signalfd(p_conn_map,
                                          &p_sd_buf_array[poll_index]))
            {
                res = RPLIB_UNSUCCESS;
                goto leave;
            }
        }
        /*
         * Case 4 - activity on child socket
         */
        if (p_sd_buf_array[poll_index].revents & (POLLIN | POLL_ERR))
        {
            handle_active_child_socket(&p_sd_buf_array[poll_index],
                                       p_conn_meta);
        }

    loop_cleanup:
        poll_index++;
        p_current_node = p_next_node;
    }
leave:
    return res;
}