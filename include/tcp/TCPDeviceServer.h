/*
 * TCPServer.h
 *
 *  Created on: Apr 4, 2016
 *      Author: james
 */

#ifndef INCLUDE_TCP_TCPDEVICESERVER_H_
#define INCLUDE_TCP_TCPDEVICESERVER_H_

#include <exception>
#include <map>
#include <string>
#include <thread>

#include "Beetle.h"

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
	TCPDeviceServer(Beetle &beetle, int port);
	virtual ~TCPDeviceServer();
private:
	Beetle &beetle;

	bool running;

	void serverDaemon(int port);
	std::thread t;

	void startTcpDeviceHelper(int clifd, struct sockaddr_in cliaddr);
	bool readParamsHelper(int clifd, int paramsLength,
			std::map<std::string,std::string> &params);
};

#endif /* INCLUDE_TCP_TCPDEVICESERVER_H_ */
