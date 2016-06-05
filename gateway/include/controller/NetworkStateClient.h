/*
 * NetworkReporter.h
 *
 *  Created on: Apr 16, 2016
 *      Author: James Hong
 */

#ifndef INCLUDE_CONTROLLER_NETWORKREPORTER_H_
#define INCLUDE_CONTROLLER_NETWORKREPORTER_H_

#include <memory>

#include "BeetleTypes.h"

/* Forward declarations */
class ControllerClient;

/*
 * Reports devices connected and disconnected.
 */
class NetworkStateClient {
public:
	NetworkStateClient(Beetle &beetle, std::shared_ptr<ControllerClient> client, int tcpPort);
	virtual ~NetworkStateClient();

	AddDeviceHandler getAddDeviceHandler();
	RemoveDeviceHandler getRemoveDeviceHandler();
	UpdateDeviceHandler getUpdateDeviceHandler();

	MapDevicesHandler getMapDevicesHandler();
	UnmapDevicesHandler getUnmapDevicesHandler();

private:
	Beetle &beetle;

	std::shared_ptr<ControllerClient> client;

	void addDeviceHelper(std::shared_ptr<Device> device);
	void updateDeviceHelper(std::shared_ptr<Device> device);
	void removeDeviceHelper(device_t d);

	bool lookupNamesAndIds(device_t from, device_t to, std::string &fromGateway, device_t &fromId,
			std::string &toGateway, device_t &toId);
};

#endif /* INCLUDE_CONTROLLER_NETWORKREPORTER_H_ */
