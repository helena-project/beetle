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
#include "HCI.h"

/*
 * Runs a peripheral and allows BLE centrals to connect to Beetle
 */
class L2capServer {
public:
	L2capServer(Beetle &beetle, std::string device);
	virtual ~L2capServer();
private:
	Beetle &beetle;

	/*
	 * Shared hci handle for centrals
	 */
	HCI hci;

	/*
	 * L2cap server and hci handle
	 */
	int serverFd;

	/*
	 * Cached packet payloads
	 */
	uint8_t advertisementDataBuf[256];
	int advertisementDataLen;

	uint8_t scanDataBuf[256];
	int scanDataLen;

	void setAdvertisingScanData();
	void startLEAdvertising(int deviceId);
	bool startLEAdvertisingHelper(int deviceHandle);

	void startL2CAPCentralHelper(int deviceId, int clifd, struct sockaddr_l2 cliaddr);
};

#endif /* L2CAP_L2CAPSERVER_H_ */
