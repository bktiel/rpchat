/** @file networking.c
 *
 * @brief Implements networking components required for program
 *
 * @par
 * COPYRIGHT NOTICE: None
 */

#include "networking.h"

#include <asm-generic/errno.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "rplib_common.h"

int
rpchat_begin_networking(unsigned int port_num,
                        int         *p_h_fd_server,
                        int         *p_h_fd_epoll)
{
    int res           = RPLIB_ERROR; // default failure in case of early term
    int h_fd_epoll    = -1;          // fd that describes epoll
    int h_sock_server = -1;
    struct epoll_event event_server;

    // create epoll fd (1 automatically resizes..)
    h_fd_epoll = epoll_create1(0);
    if (0 > h_fd_epoll)
    {
        // failure
        perror("epoll");
        goto leave;
    }

    // create server socket
    h_sock_server = rpchat_setup_server_socket(port_num);
    if (0 > h_sock_server)
    {
        // failure
        goto leave;
    }

    // tell epoll to watch server socket
    event_server.events  = EPOLLIN;
    event_server.data.fd = h_sock_server; // fd stands in place of pointer here
    if (0 > epoll_ctl(h_fd_epoll, EPOLL_CTL_ADD, h_sock_server, &event_server))
    {
        perror("server socket");
        goto leave;
    }

    // set server and epoll FDs
    *p_h_fd_server = h_sock_server;
    *p_h_fd_epoll  = h_fd_epoll;

    // return success
    res = RPLIB_SUCCESS;

leave:
    return res;
}

int
rpchat_setup_server_socket(unsigned int port_num)
{
    int                res = RPLIB_ERROR;  // initially error in case early term
    int                h_sock_server = -1; // server fd
    struct sockaddr_in addr;               // server address
    int                reuse = 1; // required to have in stack frame to ref

    // open TCP socket
    h_sock_server = socket(AF_INET, SOCK_STREAM, 0);
    if (0 > h_sock_server)
    {
        perror("sock");
        goto leave;
    }

    // sanitize addr to prevent unexpected behavior, then assign
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;           // what type of addr (ipv4)
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // what should we bind to
    addr.sin_port        = htons(port_num);   // what port to bind to

    // additional options - reuse port and addr
    if (0 > setsockopt(h_sock_server,
                       SOL_SOCKET,
                       SO_REUSEADDR,
                       (const char *)&reuse,
                       sizeof(reuse)))
    {
        perror("setsockopt");
        goto leave;
    }

    if (0 > setsockopt(h_sock_server,
                       SOL_SOCKET,
                       SO_REUSEPORT,
                       (const char *)&reuse,
                       sizeof(reuse)))
    {
        perror("setsockopt");
        goto leave;
    }

    // attempt to bind to built address
    if (0 > bind(h_sock_server, (struct sockaddr *)&addr, sizeof(addr)))
    {
        // failure
        perror("sock");
        goto leave;
    }

    // begin listening on this socket
    if (0 > listen(h_sock_server, SOMAXCONN))
    {
        // failure
        perror("sock");
        goto leave;
    }

    res = h_sock_server;
leave:
    return res;
}

int
rpchat_monitor_connections(int                 h_fd_epoll,
                           struct epoll_event *p_ret_event_buf,
                           unsigned int        max_connections)
{
    int res   = RPLIB_UNSUCCESS; // default failure in case early term
    int ready = 0;               // num of events ready

    // block until events on watched fds
    // on event - 3 possibilities: error, new client, existing client
    ready = epoll_wait(h_fd_epoll, p_ret_event_buf, max_connections, -1);

    // error state
    if (0 > ready)
    {
        // check for interrupt - helps for gdb
        if (errno == EINTR)
        {
            res = RPLIB_SUCCESS;
            goto leave;
        }
        perror("epoll");
        res = RPLIB_ERROR;
        goto leave;
    }
    res = ready;

leave:
    return res;
}

int
rpchat_accept_new_connection(unsigned int h_fd_server)
{
    struct sockaddr_in client_addr;
    socklen_t          client_addr_len = sizeof(client_addr);
    int                h_new_fd        = RPLIB_ERROR;
    int                res             = RPLIB_ERROR;

    // accept new connection
    h_new_fd = accept(
        h_fd_server, (struct sockaddr *)&client_addr, &client_addr_len);

    if (0 > h_new_fd)
    {
        perror("New connection");
        res = RPLIB_ERROR;
        goto leave;
    }
    // set nonblocking
    res = fcntl(h_new_fd, F_SETFL, fcntl(h_new_fd, F_GETFL, 0) | O_NONBLOCK);
    if (0 > res)
    {
        perror("fcntl");
    }
    res = h_new_fd;

leave:
    return res;
}

int
rpchat_close_connection(int h_fd_epoll, int h_fd)
{
    int res = RPLIB_UNSUCCESS;
    // close connection
    res = close(h_fd);
    if (RPLIB_SUCCESS != res)
    {
        goto leave;
    }
    // remove from epoll consideration
    res = epoll_ctl(h_fd_epoll, EPOLL_CTL_DEL, h_fd, NULL);
    if (RPLIB_SUCCESS != res)
    {
        goto leave;
    }
leave:
    return res;
}

int
rpchat_recv(int h_fd_client, char *p_buf, size_t len)
{
    ssize_t read_bytes = 0;

    read_bytes = recv(h_fd_client, p_buf, len, 0);

    return 0 < read_bytes ? read_bytes : RPLIB_ERROR;
}

int
rpchat_sendmsg(int h_fd_client, char *p_buf, size_t len)
{
    int sent_bytes = 0;

    sent_bytes = send(h_fd_client, p_buf, len, 0);

    return sent_bytes;
}
