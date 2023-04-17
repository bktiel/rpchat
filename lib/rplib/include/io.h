/** @file io.h
 *
 * @brief Contains i/o functionality
 *
 * @par
 * COPYRIGHT NOTICE: None
 */

#include <dirent.h>
#include <sys/stat.h>
#ifndef RPLIB_IO_H
#define RPLIB_IO_H

/**
 * Given a directory, attempt to open with the passed permission set
 * and store descriptor in p_dfd
 * @param p_path Directory path to evaluate
 * @param desired_perms Permissions (unistd.h)
 * @param p_h_dfd Pointer to memory in which to store dfd
 * @return Pointer to dir  stream on success, NULL on failure
 */
DIR *rplib_get_directory(const char *p_path, int desired_perms);

/**
 * Helper function to close directory stream
 * @param h_dirfd Non-zero directory stream
 * @return 0 on success, 1 on failure
 */
int rplib_close_dir(DIR* p_dirstream);

/**
 * Get a named file within an open directory descriptor.
 * @param p_dirstream Directory descriptor
 * @param perms Desired permissions (e.g. S_IRWXU)
 * @param mode Desired mode (e.g. O_WRONLY)
 * @param p_filename String filename to attempt to open
 * @return file descriptor on success, -1 on failure
 */
int rplib_get_file(
    DIR *p_dirstream, char *p_filename, int mode, int perms, char *p_root_path);

/**
 * Close an open file descriptor
 * @param h_filefd Non-negative file descriptor
 * @return 0 on success, 1 on failure
 */
int rplib_close_file(int h_filefd);

/**
 * Read data from given descriptor into the passed buffer
 * @param h_filefd File descriptor to read from
 * @param location Where in file to begin read
 * @param p_buf Pointer to buffer to read into
 * @param len Amount of bytes to read
 * @return Amount of bytes read, or -1 on failure
 */
int rplib_read_data(int h_filefd, size_t location, char *p_buf, size_t len);

/**
 * Write data from buffer into given descriptor
 * @param h_filefd File descriptor to write into
 * @param p_buf Pointer to buffer to write from
 * @param len Amount of bytes to write
 * @return Amount of bytes written, or -1 on failure
 */
int rplib_write_data(int h_filefd, char *p_buf, size_t len);

#endif /* RPLIB_IO_H */

/*** end of file ***/
