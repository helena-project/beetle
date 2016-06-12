/*
 * SSLConfig.cpp
 *
 *  Created on: May 1, 2016
 *      Author: James Hong
 */

#include "tcp/SSLConfig.h"

#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cassert>
#include <iostream>
#include <sstream>

#include "Debug.h"
#include "util/file.h"

bool SSLConfig::initialized = false;
std::mutex SSLConfig::initMutex;

SSLConfig::SSLConfig(bool verifyPeers, bool isServer, std::string cert, std::string key,
		std::string caCert) {
	initMutex.lock();
	if (!initialized) {
		SSL_load_error_strings();
		SSL_library_init();
		initialized = true;
	}
	initMutex.unlock();

	const SSL_METHOD *method;
	if (isServer) {
		method = TLSv1_2_server_method();
	} else {
		method = TLSv1_2_client_method();
	}
	ctx = SSL_CTX_new(method);
	SSL_CTX_load_verify_locations(ctx, caCert.c_str(), NULL);
	if (verifyPeers) {
		SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
				[](int preverify_ok, X509_STORE_CTX *ctx) -> int {
			char buf[512];
			X509 *err_cert;
			int err, depth;

			err_cert = X509_STORE_CTX_get_current_cert(ctx);
			err = X509_STORE_CTX_get_error(ctx);
			depth = X509_STORE_CTX_get_error_depth(ctx);

			/*
			 * TODO: use fields to identify gateway, controller, applications
			 */

			/*
			 * Retrieve the pointer to the SSL of the connection currently treated
			 * and the application specific data stored into the SSL object.
			 */
			X509_NAME_oneline(X509_get_subject_name(err_cert), buf, sizeof(buf));
			if (debug) {
				std::stringstream ss;
				ss << buf;
				pdebug(ss.str());
			}

			/*
			 * At this point, err contains the last verification error. We can use
			 * it for something special
			 */
			if (!preverify_ok && (err == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT))
			{
				X509_NAME_oneline(X509_get_issuer_name(ctx->current_cert), buf, sizeof(buf));
				printf("issuer= %s\n", buf);
			}

			return preverify_ok;
		});
	} else {
		SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
	}
	SSL_CTX_set_timeout(ctx, 6);
	SSL_CTX_set_verify_depth(ctx, 2);
	SSL_CTX_set_cipher_list(ctx, "HIGH");
	SSL_CTX_set_options(ctx, SSL_OP_SINGLE_DH_USE);

	if (!file_exists(cert)) {
		throw std::invalid_argument("cert not found: " + cert);
	}
	if (!file_exists(key)) {
		throw std::invalid_argument("key not found: " + key);
	}

	/* Set the key and cert */
	if (SSL_CTX_use_certificate_file(ctx, cert.c_str(), SSL_FILETYPE_PEM) < 0) {
		ERR_print_errors_fp(stderr);
		exit(EXIT_FAILURE);
	}

	if (SSL_CTX_use_PrivateKey_file(ctx, key.c_str(), SSL_FILETYPE_PEM) < 0) {
		ERR_print_errors_fp(stderr);
		exit(EXIT_FAILURE);
	}
}

SSLConfig::~SSLConfig() {
	SSL_CTX_free(ctx);
	ERR_free_strings();
	EVP_cleanup();
}

SSL_CTX *SSLConfig::getCtx() {
	return ctx;
}

