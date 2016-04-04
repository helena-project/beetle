/*
 * Device.h
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#ifndef DEVICE_H_
#define DEVICE_H_

#include <cstdint>
#include <exception>
#include <functional>
#include <map>
#include <mutex>
#include <string>

#include "../Beetle.h"
#include "../Handle.h"

class DeviceException : public std::exception {
  public:
    DeviceException(std::string msg) : msg(msg) {};
    DeviceException(const char *msg) : msg(msg) {};
    ~DeviceException() throw() {};
    const char *what() const throw() { return this->msg.c_str(); };
  private:
    std::string msg;
};

class Handle;

class Device {
public:
	Device(Beetle &beetle, device_t id);
	virtual ~Device();

	device_t getId() { return id; };
	std::string getName() { return name; };
	std::string getType() { return type; };

	int getHighestHandle();
	std::map<uint16_t, Handle *> handles;
	std::recursive_mutex handlesMutex;

	/*
	 * Unsubscribe from all of this device's handles.
	 */
	void unsubscribeAll(device_t d);

	virtual bool writeResponse(uint8_t *buf, int len) = 0;
	virtual bool writeCommand(uint8_t *buf, int len) = 0;
	virtual bool writeTransaction(uint8_t *buf, int len, std::function<void(uint8_t*, int)> cb) = 0;
	virtual int writeTransactionBlocking(uint8_t *buf, int len, uint8_t *&resp) = 0;

	virtual int getMTU() = 0;
protected:
	Beetle &beetle;

	std::string name;
	std::string type;
private:
	device_t id;
};

#endif /* DEVICE_H_ */
