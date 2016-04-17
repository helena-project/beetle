/*
 * NetworkReporter.cpp
 *
 *  Created on: Apr 16, 2016
 *      Author: james
 */

#include "../../include/control/NetworkReporter.h"

NetworkReporter::NetworkReporter(Beetle &beetle, std::string hostAndPort_) : beetle(beetle) {
	hostAndPort = hostAndPort_;
}

NetworkReporter::~NetworkReporter() {
	// TODO Auto-generated destructor stub
}

AddDeviceHandler NetworkReporter::getAddDeviceHandler() {
	return NULL;
}

RemoveDeviceHandler NetworkReporter::getRemoveDeviceHandler() {
	return NULL;
}


