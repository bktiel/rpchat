/** @file main.c
 *
 * @brief Entry point for program. Handles file input and makes necessary
 * library calls to implement Basic Chat Protocol
 *
 * @par
 * COPYRIGHT NOTICE: None
 */

#include "main.h"

#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>

#include "basic_chat.h"
#include "networking.h"
#include "rp_common.h"

/**
 * Parse command-line arguments
 * @param argc Arg count passed to program
 * @param pp_argv Arguments array
 * @param p_timeout Pointer to timeout variable in caller
 * @param p_target_directory Pointer to target directory variable in caller
 * @return 0 on success, 1 on problems
 */
static int
get_arguments(int argc, char **pp_argv, int *p_port_num, char *p_log_location)
{
    int   opt = 0;
    char *next_char; // used for strtol
    int   port_num            = 0;
    char *p_temp_log_location = NULL;

    // attempt to get arguments
    opterr = 0;
    while (-1 != (opt = getopt(argc, pp_argv, "t:l:h")))
    {
        // port number
        if ('p' == opt)
        {
            if (!optarg)
            {
                printf("Invalid Argument for -p\n");
                goto print_usage;
            }
            port_num = (int)strtol(optarg, &next_char, 10);
        }
        // log file location
        if ('l' == opt)
        {
            if (!optarg)
            {
                printf("Invalid Argument for -l\n");
                goto print_usage;
            }
            p_temp_log_location = optarg;
        }
        // help message
        if ('h' == opt)
        {
            goto print_usage;
        }
    }
    // if args not passed, set to default
    port_num = (0 == port_num) ? RPCHAT_DEFAULT_PORT : port_num;
    // commit all params to caller
    *p_port_num = port_num;
    if (NULL != p_temp_log_location)
    {
        snprintf(p_log_location, PATH_MAX, "%s", p_temp_log_location);
    }
    goto leave;
print_usage:
    fprintf(stdout,
            "Usage: \n rpchat -l[log location (defaults to stdout)] "
            "-p[host port number  "
            "(default %d)]\n",
            RPCHAT_DEFAULT_TIMEOUT);
    return RPLIB_UNSUCCESS;
leave:
    return RPLIB_SUCCESS;
}

/**
 * Entry point for application.
 * Checks arguments, validates passed log location, and begins the server
 * on the specified port
 * @param argc arg count
 * @param argv arg array
 * @return 0 on successful close, 1 on error state
 */
int
main(int argc, char **argv)
{
    struct rlimit rlim;        // used for call to getrlimit
    int res = RPLIB_UNSUCCESS; // assume error (in case of early termination)
    int port_num = 0;          // port number to host on
    long           max_descriptors = -1; // how many descriptors can
                                        // open per-process
    char *p_log_location = NULL;

    // get max descriptors
    if (0 > getrlimit(RLIMIT_NOFILE, &rlim))
    {
        perror("rlimit");
        goto leave;
    }
    max_descriptors = rlim.rlim_cur - RPCHAT_MAX_USABLE_DESCRIPTOR_OFFSET;


    // parse command-line arguments
    if (RPLIB_SUCCESS != get_arguments(argc, argv, &port_num, p_log_location))
    {
        goto leave;
    }

    // validation
    if (0 > max_descriptors)
    {
        perror("rlimit");
        goto leave;
    }

    // TODO: verify log_location perms, make call to dup

    // print args (for situational awareness)
    printf("Port: %d\n", port_num);
    printf("Log Location: %s\n",
           NULL != p_log_location ? p_log_location : "stdout");

    // begin
    res = rpchat_begin_networking(port_num, max_descriptors);

leave:
    return res;
}
