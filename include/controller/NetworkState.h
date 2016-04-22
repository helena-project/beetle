/*
 * NetworkReporter.h
 *
 *  Created on: Apr 16, 2016
 *      Author: james
 */

#ifndef INCLUDE_CONTROLLER_NETWORKREPORTER_H_
#define INCLUDE_CONTROLLER_NETWORKREPORTER_H_

#include <boost/network/protocol/http/client.hpp>
#include <string>

#include "Beetle.h"

/*
 * Reports devices connected and disconnected.
 */
class NetworkState {
public:
	NetworkState(Beetle &beetle, std::string hostAndPort);
	virtual ~NetworkState();

	AddDeviceHandler getAddDeviceHandler();
	RemoveDeviceHandler getRemoveDeviceHandler();
private:
	Beetle &beetle;

	std::string hostAndPort;

	boost::network::http::client *client;

	void addDeviceHelper(Device *device);
	void removeDeviceHelper(device_t d);
};

#endif /* INCLUDE_CONTROLLER_NETWORKREPORTER_H_ */
