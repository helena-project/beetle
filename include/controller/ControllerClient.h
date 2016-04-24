/*
 * NetworkClient.h
 *
 *  Created on: Apr 23, 2016
 *      Author: james
 */

#ifndef CONTROLLER_CONTROLLERCLIENT_H_
#define CONTROLLER_CONTROLLERCLIENT_H_

#include <boost/network/protocol/http/client.hpp>
#include <exception>
#include <string>

class ControllerException : public std::exception {
  public:
	ControllerException(std::string msg) : msg(msg) {};
	ControllerException(const char *msg) : msg(msg) {};
    ~ControllerException() throw() {};
    const char *what() const throw() { return this->msg.c_str(); };
  private:
    std::string msg;
};

class ControllerClient {
public:
	ControllerClient(std::string hostAndPort);
	ControllerClient(std::string hostAndPort, std::string cert, std::string caCerts);
	virtual ~ControllerClient();
	std::string getUrl(std::string resource);
	boost::network::http::client *getClient();
private:
	std::string hostAndPort;
	boost::network::http::client *client;
};

#endif /* CONTROLLER_CONTROLLERCLIENT_H_ */
