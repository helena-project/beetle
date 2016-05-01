/*
 * NetworkClient.h
 *
 *  Created on: Apr 23, 2016
 *      Author: james
 */

#ifndef CONTROLLER_CONTROLLERCLIENT_H_
#define CONTROLLER_CONTROLLERCLIENT_H_

#define BOOST_NETWORK_ENABLE_HTTPS

#include <boost/network/protocol/http/client.hpp>
#include <exception>
#include <string>

#include "Beetle.h"

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
	ControllerClient(Beetle &beetle, std::string hostAndPort, bool verifyPeers);
	ControllerClient(Beetle &beetle, std::string hostAndPort, std::string cert,
			std::string caCerts, bool verifyPeers);
	virtual ~ControllerClient();
	std::string getUrl(std::string resource);
	std::shared_ptr<boost::network::http::client> getClient();
	std::string getName();
private:
	Beetle &beetle;
	std::string hostAndPort;
	std::shared_ptr<boost::network::http::client> client;
};

#endif /* CONTROLLER_CONTROLLERCLIENT_H_ */
