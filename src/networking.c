/** @file networking.c
 *
 * @brief Implements networking components required for program
 *
 * @par
 * COPYRIGHT NOTICE: None
 */

#include "networking.h"

#include <malloc.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>

#include "rp_common.h"

int
rpchat_begin_networking(unsigned int port_num, unsigned int max_connections)
{
    int res        = RPLIB_UNSUCCESS; // default failure in case of early term
    int loop_res   = RPLIB_SUCCESS;   // default success
    int h_fd_epoll = -1;              // fd that describes epoll
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

    // begin awaiting events
    for (;;)
    {
        loop_res = rpchat_monitor_connections(h_fd_epoll, max_connections);
        if (RPLIB_UNSUCCESS == loop_res)
        {
            res = RPLIB_SUCCESS;
            break;
        }
        else if (RPLIB_ERROR == loop_res)
        {
            res = RPLIB_UNSUCCESS;
            break;
        }
    }

leave:
    return res;
}

int
rpchat_setup_server_socket(unsigned int port_num)
{
    int                res = RPLIB_ERROR;  // initially error in case early term
    int                h_sock_server = -1; // server fd
    struct sockaddr_in addr;               // server address

    // open TCP socket
    h_sock_server = socket(AF_INET, SOCK_STREAM, 0);
    if (0 > h_sock_server)
    {
        perror("sock");
        goto leave;
    }

    // san addr to prevent unexpected behavior, then assign
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;           // what type of addr (ipv4)
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // what should we bind to
    addr.sin_port        = htons(port_num);   // what port to bind to

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
rpchat_monitor_connections(int h_fd_epoll, int max_connections)
{
    int res   = RPLIB_UNSUCCESS;  // default failure in case early term
    int ready = 0;                // # events ready
    struct epoll_event *p_events; // buffer for events

    // allocate events buffer
    p_events = calloc(max_connections, sizeof(struct epoll_event));
    if(NULL==p_events) {
        perror("malloc");
        goto cleanup;
    }

    // block until events on watched fds
    // on event - 3 possibilities: error, new client, existing client
    ready=epoll_wait(h_fd_epoll,p_events,max_connections,-1);
    // error state
    if(0>ready) {
        perror("epoll");
        res=RPLIB_ERROR;
        goto leave;
    }
    // iterate over events
    for(int i; i<max_connections; i++) {
        // new connection
        if(p_events[i].data.)
    }


cleanup:
    free(p_events);
leave:
    return res;
}
