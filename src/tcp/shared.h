/*
 * shared.h
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#ifndef TCP_SHARED_H_
#define TCP_SHARED_H_

#include <openssl/ossl_typ.h>
#include <openssl/ssl.h>
#include <unistd.h>
#include <cstdint>

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

inline int SSL_write_all(SSL *ssl, uint8_t *buf, int len) {
	int written = 0;
	while (written < len) {
		int n = SSL_write(ssl, buf + written, len - written);
		if (n < 0) {
			return -1;
		}
		written += n;
	}
	return written;
}


#endif /* TCP_SHARED_H_ */
