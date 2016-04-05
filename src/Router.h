/*
 * Router.h
 *
 *  Created on: Mar 24, 2016
 *      Author: james
 */

#ifndef ROUTER_H_
#define ROUTER_H_

#include "Device.h"

// Extra debugging when routing.
extern bool debug_router;

class Router {
public:
	Router(Beetle &beetle);
	virtual ~Router();
	int route(uint8_t *buf, int len, device_t src);
private:
	Beetle &beetle;

	int routeFindInfo(uint8_t *buf, int len, device_t src);
	int routeFindByTypeValue(uint8_t *buf, int len, device_t src);
	int routeReadByType(uint8_t *buf, int len, device_t src);
	int routeHandleNotification(uint8_t *buf, int len, device_t src);
	int routeReadWrite(uint8_t *buf, int len, device_t src);
};

#endif /* ROUTER_H_ */
