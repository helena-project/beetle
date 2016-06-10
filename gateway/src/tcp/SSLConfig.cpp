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

#include "util/file.h"

bool SSLConfig::initialized = false;
std::mutex SSLConfig::initMutex;

SSLConfig::SSLConfig(bool verifyPeers, bool isServer, std::string cert, std::string key) {
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
	SSL_CTX_set_default_verify_paths(ctx);
	if (verifyPeers) {
		SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
	} else {
		SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
	}
	SSL_CTX_set_timeout(ctx, 6);
	SSL_CTX_set_verify_depth(ctx, 3);
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

