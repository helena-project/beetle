/*
 * BeetleDevice.h
 *
 *  Created on: Apr 3, 2016
 *      Author: root
 */

#ifndef DEVICE_BEETLEDEVICE_H_
#define DEVICE_BEETLEDEVICE_H_

#include <cstdint>
#include <functional>
#include <string>

#include "../ble/att.h"
#include "../Beetle.h"
#include "../Device.h"

/*
 * Fully synchronous simulated device for this Beetle.
 */
class BeetleDevice: public Device {
public:
	BeetleDevice(Beetle &beetle);
	virtual ~BeetleDevice();

	int getMTU() { return ATT_DEFAULT_LE_MTU; };

	bool writeResponse(uint8_t *buf, int len);
	bool writeCommand(uint8_t *buf, int len);
	bool writeTransaction(uint8_t *buf, int len, std::function<void(uint8_t*, int)> cb);
	int writeTransactionBlocking(uint8_t *buf, int len, uint8_t *&resp);
};

#endif /* DEVICE_BEETLEDEVICE_H_ */
