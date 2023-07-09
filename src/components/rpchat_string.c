/** @file rpchat_string.c
 *
 * @brief Implement rpchat string
 *
 * @par
 * COPYRIGHT NOTICE: None
 */

#include "components/rpchat_string.h"


int
rpchat_string_sanitize(rpchat_string_t *p_input_string,
                       rpchat_string_t *p_output_string,
                       bool             b_allow_ctrl)
{
    int    res               = RPLIB_UNSUCCESS;
    size_t curr_output_index = 0;
    size_t loop_index        = 0;
    char   curr_char         = 0;
    // double check lengths compliant
    p_input_string->len  = p_input_string->len < RPCHAT_MAX_STR_LENGTH
                               ? p_input_string->len
                               : RPCHAT_MAX_STR_LENGTH;
    p_output_string->len = 0;
    // ensure output clean
    memset(p_output_string, 0, p_output_string->len);
    for (loop_index = 0; loop_index < p_input_string->len; loop_index++)
    {
        curr_char = p_input_string->contents[loop_index];
        // if character meets filter rules, append to output
        if ((curr_char >= RPCHAT_FILTER_ASCII_START
             && curr_char <= RPCHAT_FILTER_ASCII_END)
            || (b_allow_ctrl
                && (RPCHAT_FILTER_ASCII_TAB == curr_char
                    || RPCHAT_FILTER_ASCII_NEWLINE == curr_char
                    || RPCHAT_FILTER_ASCII_SPACE == curr_char)))
        {
            p_output_string->contents[curr_output_index] = curr_char;
            curr_output_index++;
        }
    }
    // null-terminate if not already
    if (curr_output_index > 0
        && '\0' != p_output_string->contents[curr_output_index - 1])
    {
        p_output_string->contents[curr_output_index] = '\0';
        curr_output_index += 1;
    }
    // set length
    p_output_string->len = curr_output_index;
    // return unsuccess if string of length 1 (just terminator)
    res = p_output_string->len > 0 ? RPLIB_SUCCESS : RPLIB_UNSUCCESS;

    return res;
}
