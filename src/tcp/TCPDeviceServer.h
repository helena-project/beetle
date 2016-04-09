/*
 * TCPServer.h
 *
 *  Created on: Apr 4, 2016
 *      Author: james
 */

#ifndef TCP_TCPDEVICESERVER_H_
#define TCP_TCPDEVICESERVER_H_

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

/*
 * Run a TCP server to listen for remote Beetle clients.
 */
class TCPDeviceServer {
public:
	TCPDeviceServer(Beetle &beetle, int port, bool discover);
	virtual ~TCPDeviceServer();
private:
	Beetle &beetle;

	bool running;
	bool discover;

	void serverDaemon(int port);
	std::thread t;

	void startTcpDeviceHelper(int clifd);
	bool readParamsHelper(int clifd, int paramsLength,
			std::map<std::string,std::string> &params);
};

#endif /* TCP_TCPDEVICESERVER_H_ */
