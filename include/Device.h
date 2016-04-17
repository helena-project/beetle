/*
 * Device.h
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#ifndef DEVICE_H_
#define DEVICE_H_

#include <atomic>
#include <cstdint>
#include <exception>
#include <functional>
#include <map>
#include <mutex>
#include <string>

#include "../include/Beetle.h"
#include "../include/Handle.h"
#include "hat/HandleAllocationTable.h"

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

	enum DeviceType {
		UNKNOWN = -1,
		BEETLE_META = 0,
		LE_PERIPHERAL = 1,
		TCP_CONNECTION = 2,
		IPC_APPLICATION = 3,
		TCP_CLIENT_PROXY = 4,
		TCP_SERVER_PROXY = 5,
	};

	virtual ~Device();

	/*
	 * Get the device's unique identifier for this connection
	 * instance.
	 */
	device_t getId();

	/*
	 * Returns the device name if it has been set.
	 */
	std::string getName();

	/*
	 * Returns a enum device type.
	 */
	DeviceType getType();

	/*
	 * Returns a enum device type.
	 */
	std::string getTypeStr();

	/*
	 * Return largest, untranslated handle.
	 */
	int getHighestHandle();

	/*
	 * Handles that this device exposes to other devices as a server.
	 */
	std::map<uint16_t, Handle *> handles;
	std::recursive_mutex handlesMutex;

	/*
	 * Handle address offsets that this device is a client to.
	 */
	HandleAllocationTable *hat;
	std::mutex hatMutex;

	/*
	 * Unsubscribe from all of this device's handles.
	 */
	void unsubscribeAll(device_t d);

	/*
	 * Enqueues a response. Returns whether the response was enqueued.
	 */
	virtual bool writeResponse(uint8_t *buf, int len) = 0;

	/*
	 * Enqueues a command. Returns whether the command was enqueued.
	 */
	virtual bool writeCommand(uint8_t *buf, int len) = 0;

	/*
	 * Enqueues a transaction. The callback is called when the response is received.
	 * The pointers passed to cb do not persist after cb is done. Returns whether
	 * the transaction was enqueued. On error the first argument to cb is NULL.
	 */
	virtual bool writeTransaction(uint8_t *buf, int len, std::function<void(uint8_t*, int)> cb) = 0;

	/*
	 * Blocks until the transaction finishes. Resp is set and must be freed by the caller.
	 * Returns the length of resp. On error, resp is NULL.
	 */
	virtual int writeTransactionBlocking(uint8_t *buf, int len, uint8_t *&resp) = 0;

	/*
	 * The largest packet this device can receive. Beetle will
	 * only ever advertise the default ATT_MTU, but larger MTU
	 * can allow faster handle discovery.
	 */
	virtual int getMTU() = 0;
protected:
	/*
	 * Cannot instantiate a device directly.
	 */
	Device(Beetle &beetle);
	Device(Beetle &beetle, device_t id);

	Beetle &beetle;

	std::string name;
	DeviceType type;
private:
	device_t id;

	static std::atomic_int idCounter;
	static const std::string deviceType2Str[];
};

#endif /* DEVICE_H_ */
