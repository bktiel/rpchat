/** @file rpchat_basic_chat_util.c
 *
 * @brief Implements helper functions specific to basic chat protocol
 * implementation
 *
 * @par
 * COPYRIGHT NOTICE: None
 */

#include "rpchat_basic_chat_util.h"

#include "components/rpchat_conn_info.h"

int
rpchat_toggle_descriptor(int   h_fd_epoll,
                         int   h_toggle_fd,
                         void *p_data_ptr,
                         bool  enabled)
{
    int                res = RPLIB_UNSUCCESS;
    struct epoll_event delta_event; // contains new defs
    // set defs
    if (!enabled)
    {
        res = epoll_ctl(h_fd_epoll, EPOLL_CTL_DEL, h_toggle_fd, NULL);
        goto leave;
    }
    delta_event.events   = (EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET);
    delta_event.data.ptr = p_data_ptr;
    // mod descriptor
    epoll_ctl(h_fd_epoll, EPOLL_CTL_ADD, h_toggle_fd, &delta_event);
leave:
    return res;
}

rpchat_msg_type_t
rpchat_get_msg_type(char *p_msg_buf)
{
    int   res    = RPLIB_UNSUCCESS;
    char *opcode = p_msg_buf;
    switch (*opcode)
    {
        case 1:
            res = RPCHAT_BCP_REGISTER;
            break;
        case 2:
            res = RPCHAT_BCP_SEND;
            break;
        case 3:
            res = RPCHAT_BCP_DELIVER;
            break;
        case 4:
            res = RPCHAT_BCP_STATUS;
            break;
        default:
            res = RPLIB_UNSUCCESS;
            break;
    }
    return res;
}
