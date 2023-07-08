/** @file rpchat_file_io.c
 *
 * @brief Definitions for IO functions declared in `rpchat_file_io.h`
 * @par
 * COPYRIGHT NOTICE: None
 */

#include "rpchat_file_io.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

int
rpchat_open_log_location(char *p_log_location)
{
    int h_fd_log_loc = RPLIB_ERROR;

    // open file for write, create as necessary, with permissions 644
    h_fd_log_loc = open(p_log_location,
                        O_WRONLY | O_CREAT | O_APPEND,
                        S_IRWXU | S_IRGRP | S_IROTH);
    if (0 > h_fd_log_loc)
    {
        perror("Log Location");
        goto leave;
    }

    // duplicate stdout file descriptor
    if (dup2(h_fd_log_loc, STDOUT_FILENO) == -1)
    {
        h_fd_log_loc = RPLIB_ERROR;
        perror("Log Location");
    }

leave:
    return h_fd_log_loc;
}

int
rpchat_close_log_location(int h_fd_log_loc)
{
    return close(h_fd_log_loc);
}
