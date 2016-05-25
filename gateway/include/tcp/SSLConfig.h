/*
 * SSLConfig.h
 *
 *  Created on: May 1, 2016
 *      Author: James Hong
 */

#ifndef TCP_SSLCONFIG_H_
#define TCP_SSLCONFIG_H_

#include <openssl/ossl_typ.h>
#include <string>
#include <mutex>

class SSLConfig {
public:
	SSLConfig(bool verifyPeers, bool isServer = false, std::string cert = "", std::string key = "");
	virtual ~SSLConfig();
	SSL_CTX *getCtx();
private:
	SSL_CTX *ctx;

	static bool initialized;
	static std::mutex initMutex;
};

#endif /* TCP_SSLCONFIG_H_ */
