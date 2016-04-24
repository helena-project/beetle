/*
 * NetworkDiscovery.h
 *
 *  Created on: Apr 21, 2016
 *      Author: james
 */

#ifndef CONTROLLER_NETWORKDISCOVERY_H_
#define CONTROLLER_NETWORKDISCOVERY_H_

#include <boost/network/protocol/http/client.hpp>
#include <string>
#include <list>

#include <Beetle.h>
#include <controller/ControllerClient.h>
#include <UUID.h>

typedef struct {
	std::string name;
	device_t id;
	std::string gateway;
	std::string ip;
	int port;
} discovery_result_t;

class NetworkDiscovery {
public:
	NetworkDiscovery(ControllerClient &client);
	virtual ~NetworkDiscovery();

	/*
	 * Look for devices in the network with the uuid
	 */
	std::list<discovery_result_t> discoverByUuid(UUID uuid, bool isService = true);

	/*
	 * Look for devices in the network.
	 */
	std::list<discovery_result_t> discoverDevices();

	/*
	 * Map gateway to host and port.
	 */
private:
	ControllerClient &client;

	std::list<discovery_result_t> queryHelper(std::string resource);
};

#endif /* CONTROLLER_NETWORKDISCOVERY_H_ */
