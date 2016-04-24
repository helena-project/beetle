/*
 * NetworkReporter.h
 *
 *  Created on: Apr 16, 2016
 *      Author: james
 */

#ifndef INCLUDE_CONTROLLER_NETWORKREPORTER_H_
#define INCLUDE_CONTROLLER_NETWORKREPORTER_H_

#include "Beetle.h"
#include "ControllerClient.h"

/*
 * Reports devices connected and disconnected.
 */
class NetworkState {
public:
	NetworkState(Beetle &beetle, ControllerClient &client, int tcpPort);
	virtual ~NetworkState();

	AddDeviceHandler getAddDeviceHandler();
	RemoveDeviceHandler getRemoveDeviceHandler();
private:
	Beetle &beetle;

	ControllerClient &client;

	void addDeviceHelper(Device *device);
	void removeDeviceHelper(device_t d);
};

#endif /* INCLUDE_CONTROLLER_NETWORKREPORTER_H_ */
