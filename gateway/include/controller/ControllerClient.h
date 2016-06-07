/*
 * NetworkClient.h
 *
 *  Created on: Apr 23, 2016
 *      Author: James Hong
 */

#ifndef CONTROLLER_CONTROLLERCLIENT_H_
#define CONTROLLER_CONTROLLERCLIENT_H_

#define BOOST_NETWORK_ENABLE_HTTPS

#include <boost/network/protocol/http/client.hpp>
#include <exception>
#include <string>
#include <memory>

#include "BeetleTypes.h"

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
	ControllerClient(Beetle &beetle, std::string host, int apiPort, int ctrlPort, bool verifyPeers);
	ControllerClient(Beetle &beetle, std::string host, int apiPort, int ctrlPort, std::string cert,
			std::string caCerts, bool verifyPeers);
	virtual ~ControllerClient();
	std::string getApiUrl(std::string resource);
	std::shared_ptr<boost::network::http::client> getClient();
	std::string getName();
	std::string getHost();
	int getApiPort();
	int getCtrlPort();
	std::string getSessionToken();
	void setSessionToken(std::string token);

	static const std::string SESSION_HEADER;
private:
	Beetle &beetle;
	std::string host;
	int apiPort;
	int ctrlPort;
	std::string sessionToken;
	std::shared_ptr<boost::network::http::client> client;
};

#endif /* CONTROLLER_CONTROLLERCLIENT_H_ */
