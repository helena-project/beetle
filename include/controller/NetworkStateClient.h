/*
 * NetworkReporter.h
 *
 *  Created on: Apr 16, 2016
 *      Author: james
 */

#ifndef INCLUDE_CONTROLLER_NETWORKREPORTER_H_
#define INCLUDE_CONTROLLER_NETWORKREPORTER_H_

#include <memory>

#include "Beetle.h"

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
private:
	Beetle &beetle;

	std::shared_ptr<ControllerClient> client;

	void addDeviceHelper(Device *device);
	void updateDeviceHelper(Device *device);
	void removeDeviceHelper(device_t d);
};

#endif /* INCLUDE_CONTROLLER_NETWORKREPORTER_H_ */
