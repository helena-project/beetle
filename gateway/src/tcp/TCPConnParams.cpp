/*
 * TCPConnParams.cpp
 *
 *  Created on: Apr 23, 2016
 *      Author: James Hong
 */

#include "tcp/TCPConnParams.h"

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <map>
#include <netinet/in.h>
#include <openssl/ossl_typ.h>
#include <openssl/ssl.h>
#include <sstream>
#include <stddef.h>
#include <string>
#include <sys/select.h>
#include <sys/time.h>

#include "Debug.h"

bool readParamsHelper(SSL *ssl, int fd, std::map<std::string, std::string> &params, double timeout) {
	time_t startTime = time(NULL);

	struct timeval defaultTimeout;
	defaultTimeout.tv_sec = 0;
	defaultTimeout.tv_usec = 100000;

	fd_set fdSet;
	FD_ZERO(&fdSet);
	FD_SET(fd, &fdSet);

	uint32_t paramsLen;
	int paramsLenRead = 0;
	while (paramsLenRead < (int)sizeof(uint32_t)) {
		if (difftime(time(NULL), startTime) > timeout) {
			if (debug) {
				pdebug("timed out reading connection parameters length");
			}
			return false;
		}

		int result = SSL_pending(ssl);

		if (result <= 0) {
			struct timeval timeout = defaultTimeout;
			fd_set readFds = fdSet;
			fd_set exceptFds = fdSet;
			result = select(fd+1, &readFds, NULL, &exceptFds, &timeout);
			if (result < 0) {
				if (debug) {
					std::stringstream ss;
					ss << "select failed : " << strerror(errno);
					pdebug(ss.str());
				}
				return false;
			}
		}

		if (result > 0) {
			int n = SSL_read(ssl, ((uint8_t *)&paramsLen) + paramsLenRead, (int)sizeof(uint32_t) - paramsLenRead);
			if (n <= 0) {
				if (debug) {
					pdebug("could not finish reading tcp connection parameters length");
				}
				return false;
			}
			paramsLenRead += n;
		}
	}

	/*
	 * Read params from the client.
	 */
	paramsLen = ntohl(paramsLen);
	if (debug) {
		pdebug("expecting " + std::to_string(paramsLen) + " bytes of parameters");
	}

	int bytesRead = 0;
	char lineBuffer[2048];
	int lineIndex = 0;
	while (bytesRead < (int)paramsLen) {
		if (difftime(time(NULL), startTime) > timeout) {
			if (debug) {
				pdebug("timed out reading connection parameters");
			}
			return false;
		}

		int result = SSL_pending(ssl);

		if (result <= 0) {
			struct timeval timeout = defaultTimeout;
			fd_set readFds = fdSet;
			fd_set exceptFds = fdSet;
			result = select(fd+1, &readFds, NULL, &exceptFds, &timeout);
			if (result < 0) {
				if (debug) {
					std::stringstream ss;
					ss << "select failed : " << strerror(errno);
					pdebug(ss.str());
				}
				return false;
			}
		}

		if (result > 0) {
			char ch;
			int n = SSL_read(ssl, &ch, 1);
			if (n <= 0) {
				if (debug) {
					pdebug("could not finish reading tcp connection parameters");
				}
				return false;
			}

			/* advance */
			bytesRead += n;

			if (ch == '\n') {
				lineBuffer[lineIndex] = '\0';
				lineIndex = 0;
				std::string line = std::string(lineBuffer);
				if (line.length() == 0) {
					continue;
				}
				size_t splitIdx = line.find(' ');
				if (splitIdx == std::string::npos) {
					if (debug) {
						pdebug("unable to parse parameters: " + line);
					}
					return false;
				}
				std::string paramName = line.substr(0, splitIdx);
				std::string paramValue = line.substr(splitIdx + 1, line.length());
				params[paramName] = paramValue;
			} else if (ch != '\0') {
				lineBuffer[lineIndex] = ch;
				lineIndex++;
			}
		}
	}

	if (lineIndex != 0) {
		lineBuffer[lineIndex] = '\0';
		std::string line = std::string(lineBuffer);
		size_t splitIdx = line.find(' ');
		if (splitIdx == std::string::npos) {
			if (debug) {
				pdebug("unable to parse parameters: " + line);
			}
			return false;
		}
		std::string paramName = line.substr(0, splitIdx);
		std::string paramValue = line.substr(splitIdx + 1, line.length());
		params[paramName] = paramValue;
	}
	return true;
}

