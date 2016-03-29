/*
 * Router.h
 *
 *  Created on: Mar 24, 2016
 *      Author: james
 */

#ifndef ROUTER_H_
#define ROUTER_H_

#include "device/Device.h"

class Router {
public:
	Router(Beetle &beetle);
	virtual ~Router();
	int route(uint8_t *buf, int len, device_t src);
private:
	Beetle &beetle;

	int routeFindInfo(uint8_t *buf, int len, device_t src);
	int routeFindByType(uint8_t *buf, int len, device_t src);
	int routeReadByType(uint8_t *buf, int len, device_t src);
};

#endif /* ROUTER_H_ */
