/*
 * NetworkReporter.h
 *
 *  Created on: Apr 16, 2016
 *      Author: james
 */

#ifndef INCLUDE_CONTROLLER_NETWORKREPORTER_H_
#define INCLUDE_CONTROLLER_NETWORKREPORTER_H_

#include <string>

#include <boost/network/protocol/http/client.hpp>
#include <exception>

#include "Beetle.h"

// Print debug information about the control-plane
extern bool debug_network;

class NetworkException : public std::exception {
  public:
	NetworkException(std::string msg) : msg(msg) {};
	NetworkException(const char *msg) : msg(msg) {};
    ~NetworkException() throw() {};
    const char *what() const throw() { return this->msg.c_str(); };
  private:
    std::string msg;
};

/*
 * Reports devices connected and disconnected.
 */
class NetworkReporter {
public:
	NetworkReporter(Beetle &beetle, std::string hostAndPort);
	virtual ~NetworkReporter();

	AddDeviceHandler getAddDeviceHandler();
	RemoveDeviceHandler getRemoveDeviceHandler();
private:
	Beetle &beetle;

	std::string hostAndPort;

	boost::network::http::client client;

	void addDeviceHelper(Device *device);
	void removeDeviceHelper(device_t d);
};

#endif /* INCLUDE_CONTROLLER_NETWORKREPORTER_H_ */
