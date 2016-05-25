/*
 * NetworkDiscovery.h
 *
 *  Created on: Apr 21, 2016
 *      Author: James Hong
 */

#ifndef CONTROLLER_NETWORKDISCOVERYCLIENT_H_
#define CONTROLLER_NETWORKDISCOVERYCLIENT_H_

#include <string>
#include <list>
#include <memory>

#include "BeetleTypes.h"
#include "UUID.h"

/* Forward declarations */
class ControllerClient;

typedef struct {
	std::string name;
	device_t id;
	std::string gateway;
	std::string ip;
	int port;
} discovery_result_t;

class NetworkDiscoveryClient {
public:
	NetworkDiscoveryClient(Beetle &beetle, std::shared_ptr<ControllerClient> client);
	virtual ~NetworkDiscoveryClient();

	/*
	 * Look for devices in the network with the uuid
	 */
	bool discoverByUuid(UUID uuid, std::list<discovery_result_t> &ret, bool isService = true, device_t d = -1);

	/*
	 * Look for devices in the network.
	 */
	bool discoverDevices(std::list<discovery_result_t> &ret);

	/*
	 * Look for gateway in the network.
	 */
	bool findGatewayByName(std::string name, std::string &ip, int &port);

private:
	Beetle &beetle;

	std::shared_ptr<ControllerClient> client;

	bool queryHelper(std::string resource, std::list<discovery_result_t> &ret);
};

#endif /* CONTROLLER_NETWORKDISCOVERYCLIENT_H_ */
