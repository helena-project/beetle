/*
 * TCPServer.h
 *
 *  Created on: Apr 4, 2016
 *      Author: james
 */

#ifndef SYNC_TCPSERVER_H_
#define SYNC_TCPSERVER_H_

#include <exception>
#include <string>
#include <thread>

#include "../Beetle.h"

class ServerException : public std::exception {
  public:
	ServerException(std::string msg) : msg(msg) {};
	ServerException(const char *msg) : msg(msg) {};
    ~ServerException() throw() {};
    const char *what() const throw() { return this->msg.c_str(); };
  private:
    std::string msg;
};

class TCPDeviceServer {
public:
	TCPDeviceServer(Beetle &beetle, int port);
	virtual ~TCPDeviceServer();
private:
	Beetle &beetle;

	bool running;

	void serverDaemon(int port);
	std::thread t;
};

#endif /* SYNC_TCPSERVER_H_ */
