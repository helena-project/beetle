/*
 * L2capServer.h
 *
 *  Created on: Jun 22, 2016
 *      Author: James Hong
 */

#ifndef L2CAP_L2CAPSERVER_H_
#define L2CAP_L2CAPSERVER_H_

#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>

#include "BeetleTypes.h"

class L2CAPServer {
public:
	L2CAPServer(Beetle &beetle);
	virtual ~L2CAPServer();
private:
	Beetle &beetle;

	int fd;

	uint8_t advertisementDataBuf[256];
	int advertisementDataLen;

	uint8_t scanDataBuf[256];
	int scanDataLen;

	static void startL2capCentralHelper(Beetle &beetle, int clifd, struct sockaddr_l2 cliaddr);
};

#endif /* L2CAP_L2CAPSERVER_H_ */
