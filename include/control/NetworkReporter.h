/*
 * NetworkReporter.h
 *
 *  Created on: Apr 16, 2016
 *      Author: james
 */

#ifndef INCLUDE_CONTROL_NETWORKREPORTER_H_
#define INCLUDE_CONTROL_NETWORKREPORTER_H_

#include <string>

#include "../Beetle.h"

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
};

#endif /* INCLUDE_CONTROL_NETWORKREPORTER_H_ */
