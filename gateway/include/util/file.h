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

inline bool file_exists(const std::string &name) {
    return (access(name.c_str(), F_OK) != -1);
}

#endif /* UTIL_FILE_H_ */
