/** @file rpchat_file_io.h
*
* @brief Declares all required items for file IO for BCP implementation
*
* @par
* COPYRIGHT NOTICE: None
*/

#ifndef RPCHAT_RPCHAT_FILE_IO_H
#define RPCHAT_RPCHAT_FILE_IO_H

#include "rplib_common.h"

/**
 * Validate passed log location and prepare it for writing
 * @param p_log_location Pointer to null-terminated char buf containing log
 * location
 * @return File Descriptor for log location file, RPLIB_ERR on failure
 */
int
rpchat_open_log_location(char *p_log_location);

/**
 * Close log location file descriptor
 * @param h_fd_log_loc Log location file descriptor
 * @return RPLIB_SUCCESS on no issues, otherwise RPLIB_ERR
 */
int
rpchat_close_log_location(int h_fd_log_loc);

#endif // RPCHAT_RPCHAT_FILE_IO_H
