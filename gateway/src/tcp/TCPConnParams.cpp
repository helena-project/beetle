/*
 * TCPConnParams.cpp
 *
 *  Created on: Apr 23, 2016
 *      Author: James Hong
 */

#include <tcp/TCPConnParams.h>

#include <stddef.h>
#include <map>
#include <string>
#include <openssl/ossl_typ.h>
#include <openssl/ssl.h>

#include "Debug.h"

bool readParamsHelper(SSL *ssl, int paramsLen, std::map<std::string, std::string> &params) {
	int bytesRead = 0;
	char lineBuffer[2048];
	int lineIndex = 0;
	while (bytesRead < paramsLen) {
		char ch;
		int n = SSL_read(ssl, &ch, 1);
		if (n < 0) {
			if (debug) {
				pdebug("could not finish reading tcp connection parameters");
			}
			return false;
		} else if (n == 0) {
			continue;
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



