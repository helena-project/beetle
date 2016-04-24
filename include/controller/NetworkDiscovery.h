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
	bool discoverByUuid(UUID uuid, std::list<discovery_result_t> &ret, bool isService = true);

	/*
	 * Look for devices in the network.
	 */
	bool discoverDevices(std::list<discovery_result_t> &ret);

	/*
	 * Look for gateway in the network.
	 */
	bool findGatewayByName(std::string name, std::string &ip, int &port);

private:
	ControllerClient &client;

	bool queryHelper(std::string resource, std::list<discovery_result_t> &ret);
};

#endif /* CONTROLLER_NETWORKDISCOVERY_H_ */
