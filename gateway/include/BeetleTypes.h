/*
 * BeetleTypes.h
 *
 *  Created on: May 23, 2016
 *      Author: James Hong
 */

#ifndef BEETLETYPES_H_
#define BEETLETYPES_H_

#include <functional>

/*
 * Forward declaration of Beetle.
 */
class Beetle;
class Device;
class Handle;

/*
 * Id representing a local virtual device instance.
 */
typedef long device_t;
const device_t BEETLE_RESERVED_DEVICE = 0;
const device_t NULL_RESERVED_DEVICE = -1;

typedef std::function<void(device_t d)> AddDeviceHandler;
typedef std::function<void(device_t d)> RemoveDeviceHandler;
typedef std::function<void(device_t d)> UpdateDeviceHandler;

#endif /* BEETLETYPES_H_ */
