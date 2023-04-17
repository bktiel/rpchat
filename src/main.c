/** @file main.c
 *
 * @brief Entry point for program. Handles file input and makes necessary
 * library calls to implement TFTP server (RFC 1350)
 *
 * @par
 * COPYRIGHT NOTICE: None
 */

#include "main.h"

#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signalfd.h>

#include "io.h"
#include "networking.h"
#include "rp_common.h"
#include "tftp.h"

/**
 * Parse command-line arguments
 * @param argc Arg count passed to program
 * @param pp_argv Arguments array
 * @param p_timeout Pointer to timeout variable in caller
 * @param p_target_directory Pointer to target directory variable in caller
 * @return 0 on success, 1 on problems
 */
static int
get_arguments(int    argc,
              char **pp_argv,
              int   *p_timeout,
              char  *p_target_directory)
{
    int   opt = 0;
    char *next_char; // used for strtol
    int   timeout    = 0;
    char *target_dir = NULL;

    // attempt to get arguments
    opterr = 0;
    while (-1 != (opt = getopt(argc, pp_argv, "t:d:h")))
    {
        if ('t' == opt)
        {
            if (!optarg)
            {
                printf("Invalid Argument for -t\n");
                goto print_usage;
            }
            timeout = (int)strtol(optarg, &next_char, 10);
        }
        if ('d' == opt)
        {
            if (!optarg)
            {
                printf("Invalid Argument for -d\n");
                goto print_usage;
            }
            target_dir = optarg;
        }
        if ('h' == opt)
        {
            goto print_usage;
        }
    }
    // if optional params not set, set to default
    timeout = (0 == timeout) ? RPTFTP_DEFAULT_TIMEOUT : timeout;
    // commit all params to caller
    *p_timeout = timeout;
    if (NULL == target_dir)
    {
        target_dir = getenv("HOME");
        if(NULL==target_dir)
        {
            goto print_usage;
        }
    }
    snprintf(p_target_directory, PATH_MAX, "%s", target_dir);
    goto leave;
print_usage:
    fprintf(stdout,
            "Usage: \n rptftp -d[hosted directory (defaults to $HOME)] "
            "-t[timeout in seconds"
            "(default %d)]\n",
            RPTFTP_DEFAULT_TIMEOUT);
    return RPLIB_UNSUCCESS;
leave:
    return RPLIB_SUCCESS;
}

/**
 * Opens the server and begin accepting connections
 * @param port_num Int port number
 * @param h_dirfd Non-zero int directory file descriptor
 * @return 0 on success, 1 on less-than-success
 */
static int
begin_networking(int port_num, size_t timeout, DIR* p_dirstream, char* p_root_path)
{
    int                 res = RPLIB_UNSUCCESS;
    rptftp_conn_map_t   conn_map;  // tracker for connection objects
    sigset_t            fd_sigset; // sigset for created signal fd
    int                 h_server_sd = -1;

    h_server_sd = rptftp_create_new_udp_socket(port_num);
    if (0 > h_server_sd)
    {
        fprintf(stderr, "Unable to open socket on %d\n", port_num);
        goto leave;
    }

    // create connection tracker
    if (RPLIB_SUCCESS != rptftp_conn_map_initialize(&conn_map, timeout))
    {
        fprintf(stderr, "Unable to create connection map.\n");
        goto leave;
    }
    conn_map.p_dirstream=p_dirstream;
    strncpy(conn_map.dir_path,p_root_path,PATH_MAX);

    // add server socket conn_map
    rptftp_conn_map_add_conn(&conn_map, h_server_sd);

    // add signal fd, catch these signals via poll
    sigemptyset(&fd_sigset);
    sigaddset(&fd_sigset, SIGINT);
    sigaddset(&fd_sigset, SIGUSR1);
    // disable normal handling
    if (-1 == sigprocmask(SIG_BLOCK, &fd_sigset, NULL))
    {
        goto cleanup;
    }
    res = signalfd(-1, &fd_sigset, 0);
    if (-1 == res)
    {
        goto cleanup;
    }

    // add signal descriptor at index 1, should be RPTFTP_MAP_SIGNAL_INDEX
    rptftp_conn_map_add_conn(&conn_map, res);

    // begin accepting connections
    for (;;)
    {
        res = rptftp_monitor_sockets(&conn_map);
        if (RPLIB_SUCCESS != res)
            break;
    }

cleanup:
    rptftp_conn_map_destroy(&conn_map);
leave:
    return res;
}

/**
 * Entry point for application.
 * Checks arguments, validates passed directory, opens listening server on
 * port 69
 * @param argc arg count
 * @param argv arg array
 * @return 0 on successful close, 1 on error state
 */
int
main(int argc, char **argv)
{
    char target_dir[PATH_MAX]; // directory to serve
    int  timeout     = 0;      // timeout for connections
    DIR *p_dirstream = NULL;
    int  res         = RPLIB_UNSUCCESS;

    // parse command-line arguments
    if (RPLIB_SUCCESS != get_arguments(argc, argv, &timeout, target_dir))
    {
        goto leave;
    }

    printf("Timeout: %d\n", timeout);
    printf("Serving: %s\n", target_dir);

    // get directory that is served
    p_dirstream= rplib_get_directory(target_dir, (R_OK | W_OK));
    if (!p_dirstream)
    {
        fprintf(stderr, "Unable to open passed directory.\n");
        goto leave;
    }

    // set-up "base" socket on port 69 IAW RFC 1350
    res = begin_networking(
        RPTFTP_LISTEN_PORT, timeout, p_dirstream, target_dir);


leave:
    return res;
}