/*
 * Router.h
 *
 *  Created on: Mar 24, 2016
 *      Author: james
 */

#ifndef ROUTER_H_
#define ROUTER_H_

#include "Device.h"

class Router {
public:
	Router(Beetle &beetle_) : beetle{beetle_} {};
	virtual ~Router();
	int route(char *buf, int len, device_t src);
private:
	Beetle &beetle;

	int routeFindInfo(char *buf, int len, device_t src);
	int routeFindByType(char *buf, int len, device_t src);
	int routeReadByType(char *buf, int len, device_t src);
};

#endif /* ROUTER_H_ */
