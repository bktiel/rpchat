/** @file rpchat_string.h
 *
 * @brief Implement rpchat string
 *
 * @par
 * COPYRIGHT NOTICE: None
 */

#ifndef RPCHAT_RPCHAT_STRING_H
#define RPCHAT_RPCHAT_STRING_H

#include <string.h>
#include <stdbool.h>
#include <sys/types.h>

#include "components/rpchat_string.h"
#include "rplib_common.h"

#define RPCHAT_MAX_STR_LENGTH 4095
#define RPCHAT_FILTER_ASCII_START   33
#define RPCHAT_FILTER_ASCII_SPACE   32
#define RPCHAT_FILTER_ASCII_NEWLINE 10
#define RPCHAT_FILTER_ASCII_TAB     9
#define RPCHAT_FILTER_ASCII_END     126

typedef struct rpchat_basic_chat_string
{
    u_int16_t len;
    char      contents[RPCHAT_MAX_STR_LENGTH];
} rpchat_string_t;

/**
 * Sanitize a string to only printable characters
 * @param p_input_string Pointer to input string
 * @param p_output_string Pointer to string to store output
 * @param b_allow_ctrl If true, allow control characters like \n,\t,\w;
 * otherwise, only match printable ascii (excl. space)
 * @return RPLIB_SUCCESS if no issues; otherwise, RPLIB_UNSUCCESS
 */
int rpchat_string_sanitize(rpchat_string_t *p_input_string,
                           rpchat_string_t *p_output_string,
                           bool             b_allow_ctrl);

#endif // RPCHAT_RPCHAT_STRING_H
