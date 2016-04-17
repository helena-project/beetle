/*
 * shared.h
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#ifndef DEVICE_SOCKET_SHARED_H_
#define DEVICE_SOCKET_SHARED_H_

inline int write_all(int fd, uint8_t *buf, int len) {
	int written = 0;
	while (written < len) {
		int n = write(fd, buf + written, len - written);
		if (n < 0) {
			return -1;
		}
		written += n;
	}
	return written;
}


#endif /* DEVICE_SOCKET_SHARED_H_ */
