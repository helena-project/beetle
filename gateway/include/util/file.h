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
#include <exception>

class ParseExecption : public std::exception {
public:
	ParseExecption(std::string msg) : msg(msg) {};
	ParseExecption(const char *msg) : msg(msg) {};
	~ParseExecption() throw() {};
	const char *what() const throw() { return this->msg.c_str(); };
private:
	std::string msg;
};

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
