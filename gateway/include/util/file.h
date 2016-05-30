/*
 * file.h
 *
 *  Created on: May 23, 2016
 *      Author: James Hong
 */

#ifndef UTIL_FILE_H_
#define UTIL_FILE_H_

#include <unistd.h>
#include <string>
#include <fcntl.h>

inline bool file_exists(const std::string &name) {
    return (access(name.c_str(), F_OK) != -1);
}

inline bool fd_set_blocking(int fd, bool blocking) {
    /* Save the current flags */
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        return false;
    }

    if (blocking) {
        flags &= ~O_NONBLOCK;
    } else {
        flags |= O_NONBLOCK;
    }
    return fcntl(fd, F_SETFL, flags) != -1;
}

#endif /* UTIL_FILE_H_ */
