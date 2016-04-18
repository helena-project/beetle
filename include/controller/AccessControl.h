/*
 * AccessControl.h
 *
 *  Created on: Apr 18, 2016
 *      Author: james
 */

#ifndef CONTROLLER_ACCESSCONTROL_H_
#define CONTROLLER_ACCESSCONTROL_H_

#include <boost/network/protocol/http/client.hpp>
#include <string>

#include <Beetle.h>

class AccessControl {
public:
	AccessControl(Beetle &beetle, std::string hostAndPort);
	virtual ~AccessControl();

	/*
	 * Caller should hold at least a read lock on devices.
	 */
	bool canMap(Device *from, Device *to);
private:
	Beetle &beetle;

	std::string hostAndPort;

	boost::network::http::client client;

	bool handleCanMapResponse(Device *from, Device *to, std::string body);
};

#endif /* CONTROLLER_ACCESSCONTROL_H_ */
