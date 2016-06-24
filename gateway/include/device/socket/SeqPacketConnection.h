/*
 * SEQPacketConnection.h
 *
 *  Created on: Jun 24, 2016
 *      Author: james
 */

#ifndef DEVICE_SOCKET_SEQPACKETCONNECTION_H_
#define DEVICE_SOCKET_SEQPACKETCONNECTION_H_

#include <atomic>
#include <cstdint>
#include <list>

#include "BeetleTypes.h"
#include "device/socket/shared.h"
#include "device/VirtualDevice.h"
#include "device/socket/shared.h"
#include "sync/Countdown.h"

class SeqPacketConnection: public VirtualDevice {
public:
	virtual ~SeqPacketConnection();

protected:
	SeqPacketConnection(Beetle &beetle, int sockfd, bool isEndpoint,
			std::list<delayed_packet_t> delayedPackets);

	bool write(uint8_t *buf, int len);
	void startInternal();
private:
	int sockfd;

	std::atomic_bool stopped;
	void stopInternal();

	std::list<delayed_packet_t> delayedPackets;

	Countdown pendingWrites;
};

#endif /* DEVICE_SOCKET_SEQPACKETCONNECTION_H_ */
