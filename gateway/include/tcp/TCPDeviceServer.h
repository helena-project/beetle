/*
 * TCPServer.h
 *
 *  Created on: Apr 4, 2016
 *      Author: James Hong
 */

#ifndef INCLUDE_TCP_TCPDEVICESERVER_H_
#define INCLUDE_TCP_TCPDEVICESERVER_H_

#include <exception>
#include <openssl/ossl_typ.h>
#include <string>
#include <thread>
#include <memory>

#include "BeetleTypes.h"
#include "tcp/SSLConfig.h"

class ServerException : public std::exception {
  public:
	ServerException(std::string msg) : msg(msg) {};
	ServerException(const char *msg) : msg(msg) {};
    ~ServerException() throw() {};
    const char *what() const throw() { return this->msg.c_str(); };
  private:
    std::string msg;
};

/*
 * Run a TCP server to listen for remote Beetle clients.
 */
class TCPDeviceServer {
public:
	TCPDeviceServer(Beetle &beetle, SSLConfig *sslConfig, int port);
	virtual ~TCPDeviceServer();
private:
	Beetle &beetle;
	std::unique_ptr<SSLConfig> sslConfig;

	int sockfd;

	bool serverRunning;
	std::thread daemonThread;
	void serverDaemon(int port);

	void startTcpDeviceHelper(SSL *ssl, int clifd, struct sockaddr_in cliaddr);
};

#endif /* INCLUDE_TCP_TCPDEVICESERVER_H_ */
