/** @file rp_common.h
 *
 * @brief Implements convenience variables useful to multiple projects
 *
 * @par
 * COPYRIGHT NOTICE: None
 */

#ifndef RPLIB_RP_COMMON_H
#define RPLIB_RP_COMMON_H

#include "stdio.h"


#ifndef NDEBUG
#define RPLIB_DEBUG_PRINTF(format, ...) printf(format, __VA_ARGS__)
#else
#define RPLIB_DEBUG_PRINTF(format, ...) \
    do                            \
    {                             \
    } while (0);
#endif

#define RPLIB_SET_NEGATIVE(num) ((num>0)?-num:num)
#define RPLIB_GET_ABSOLUTE(num) (num*((num>0)-(num<0)))

#define RPLIB_IS_BIG_ENDIAN (!*(unsigned char *)&(uint16_t){1})


typedef enum
{
    RPLIB_ERROR  = -1, // routine encountered unexpected behavior
    RPLIB_SUCCESS = 0,  // routine was successful
    RPLIB_UNSUCCESS = 1,  // routine was not successful
} return_code_t;

#endif // RPLIB_RP_COMMON_H

/*** end of file ***/
