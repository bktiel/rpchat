/** @file io.c
 *
 * @brief Contains functionality for file I/O
 *
 * @par
 * COPYRIGHT NOTICE: None
 */

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "rp_common.h"

DIR *
rplib_get_directory(const char *p_path, int desired_perms)
{
    struct stat dirstat;
    DIR        *p_dir = NULL;
    char        full_path[PATH_MAX];
    errno           = 0;
    assert(p_path);

    // expand p_path
    if (!realpath(p_path, full_path))
    {
        goto leave;
    }

    // must have perms on directories
    if (0 != access(full_path, desired_perms))
    {
        goto leave;
    }

    p_dir = opendir(full_path);
    if (p_dir == NULL)
    {
        // check write permissions
        goto cleanup;
    }

    // check props
    fstat(dirfd(p_dir), &dirstat);
    goto leave;
cleanup:
    closedir(p_dir);
leave:
    return (!p_dir) ? NULL : p_dir;
}

int
rplib_close_dir(DIR *p_dirstream)
{
    int res = RPLIB_SUCCESS;
    assert(p_dirstream);
    errno = 0;
    closedir(p_dirstream);
    if (errno)
        res = RPLIB_UNSUCCESS;
    return res;
}

int
rplib_get_file(
    DIR *p_dirstream, char *p_filename, int mode, int perms, char *p_root_path)
{
    char        full_file_path[PATH_MAX];
    char        abs_file_path[PATH_MAX];
    char       *p_rel_path;
    int         res  = RPLIB_ERROR;
    int         h_fd = dirfd(p_dirstream);
    struct stat item_info;
    assert(p_dirstream);
    // reset errno
    errno = 0;
    // get full filename
    snprintf(full_file_path, PATH_MAX, "%s/%s", p_root_path, p_filename);
    if(!realpath(full_file_path, abs_file_path))
    {
        goto leave;
    }

    // ensure abs path is inside directory
    if (0 != strncmp(p_root_path, abs_file_path, strlen(p_root_path)))
    {
        res = RPLIB_ERROR;
        goto leave;
    }
    // get relative filename
    //    token = strtok(abs_file_path, "/");
    //    while (NULL != token)
    //    {
    //        last_token = token;
    //        token      = strtok(NULL, "/");
    //    }
    p_rel_path = (char *)abs_file_path + strlen(p_root_path) + 1;
    // attempt to open file
    h_fd = openat(h_fd, p_rel_path, mode, perms);
    if (0 > h_fd)
    {
        res = RPLIB_ERROR;
        goto leave;
    }
    res = h_fd;
leave:
    return res;
}

int
rplib_close_file(int h_filefd)
{
    int res = RPLIB_SUCCESS;
    errno   = 0;
    close(RPLIB_GET_ABSOLUTE(h_filefd));
    if (errno)
        res = RPLIB_UNSUCCESS;
    return res;
}

int
rplib_write_data(int h_filefd, char *p_buf, size_t len)
{
    ssize_t res = RPLIB_ERROR;
    errno       = 0;
    res         = write(h_filefd, p_buf, len);
    return res;
}

int
rplib_read_data(int h_filefd, size_t location, char *p_buf, size_t len)
{
    ssize_t res = RPLIB_ERROR;
    errno       = 0;
    // seek
    lseek(h_filefd, location, SEEK_SET);
    res = read(h_filefd, p_buf, len);
    return (RPLIB_ERROR != res ? res : RPLIB_ERROR);
}
