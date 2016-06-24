/*
 * L2capServer.cpp
 *
 *  Created on: Jun 22, 2016
 *      Author: James Hong
 */

#include "l2cap/L2capServer.h"

#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <boost/thread/lock_types.hpp>
#include <boost/thread/pthread/shared_mutex.hpp>
#include <sys/socket.h>
#include <cassert>
#include <exception>
#include <memory>
#include <string>
#include <stdio.h>
#include <sstream>
#include <unistd.h>
#include <sys/ioctl.h>
#include <thread>

#include "ble/helper.h"
#include "Beetle.h"
#include "Debug.h"
#include "device/socket/LEDevice.h"
#include "sync/SocketSelect.h"
#include "sync/ThreadPool.h"

// Set advertising interval to 100 ms
// Note: 0x00A0 * 0.625ms = 100ms
static int hci_le_set_advertising_parameters(int dd, int to) {
	struct hci_request rq;
	le_set_advertising_parameters_cp adv_params_cp;
	uint8_t status;

	memset(&adv_params_cp, 0, sizeof(adv_params_cp));
	adv_params_cp.min_interval = htobs(0x00A0);
	adv_params_cp.max_interval = htobs(0x00A0);
	adv_params_cp.chan_map = 7;

	memset(&rq, 0, sizeof(rq));
	rq.ogf = OGF_LE_CTL;
	rq.ocf = OCF_LE_SET_ADVERTISING_PARAMETERS;
	rq.cparam = &adv_params_cp;
	rq.clen = LE_SET_ADVERTISING_PARAMETERS_CP_SIZE;
	rq.rparam = &status;
	rq.rlen = 1;

	if (hci_send_req(dd, &rq, to) < 0) {
		return -1;
	}

	if (status) {
		errno = EIO;
		return -1;
	}

	return 0;
}

static int hci_le_set_advertising_data(int dd, uint8_t* data, uint8_t length, int to) {
	struct hci_request rq;
	le_set_advertising_data_cp data_cp;
	uint8_t status;

	memset(&data_cp, 0, sizeof(data_cp));
	data_cp.length = length;
	memcpy(&data_cp.data, data, sizeof(data_cp.data));

	memset(&rq, 0, sizeof(rq));
	rq.ogf = OGF_LE_CTL;
	rq.ocf = OCF_LE_SET_ADVERTISING_DATA;
	rq.cparam = &data_cp;
	rq.clen = LE_SET_ADVERTISING_DATA_CP_SIZE;
	rq.rparam = &status;
	rq.rlen = 1;

	if (hci_send_req(dd, &rq, to) < 0) {
		return -1;
	}

	if (status) {
		errno = EIO;
		return -1;
	}

	return 0;
}

static int hci_le_set_scan_response_data(int dd, uint8_t* data, uint8_t length, int to) {
	struct hci_request rq;
	le_set_scan_response_data_cp data_cp;
	uint8_t status;

	memset(&data_cp, 0, sizeof(data_cp));
	data_cp.length = length;
	memcpy(&data_cp.data, data, sizeof(data_cp.data));

	memset(&rq, 0, sizeof(rq));
	rq.ogf = OGF_LE_CTL;
	rq.ocf = OCF_LE_SET_SCAN_RESPONSE_DATA;
	rq.cparam = &data_cp;
	rq.clen = LE_SET_SCAN_RESPONSE_DATA_CP_SIZE;
	rq.rparam = &status;
	rq.rlen = 1;

	if (hci_send_req(dd, &rq, to) < 0) {
		return -1;
	}

	if (status) {
		errno = EIO;
		return -1;
	}

	return 0;
}

L2capServer::L2capServer(Beetle &beetle, std::string device) : beetle(beetle) {
	int deviceId = HCI::getHCIDeviceId(device);
	if (deviceId == HCI::getDefaultHCIDeviceId()) {
		pwarn("acting as a peripheral on the default interface may have side effects");
	}

	hci_dev_info hciDevInfo;
	if (hci_devinfo(deviceId, &hciDevInfo) < 0) {
		throw std::runtime_error("failed to obtain hci device info");
	}

	callbackDeviceHandle = hci_open_dev(deviceId);
	if (callbackDeviceHandle < 0) {
		throw std::runtime_error("failed to obtain hci device handle");
	}

	std::cout << "advertising on: " << device << std::endl;
	std::cout << "starting l2cap peripheral:" << std::endl
			<< "  name: " << hciDevInfo.name << std::endl
			<< "  bdaddr: " << ba2str_cpp(hciDevInfo.bdaddr) << std::endl;

	struct sockaddr_l2 serv_addr = { 0 };
	serv_addr.l2_family = AF_BLUETOOTH;
	serv_addr.l2_bdaddr = hciDevInfo.bdaddr;
	serv_addr.l2_cid = htobs(ATT_CID);
	serv_addr.l2_bdaddr_type = hciDevInfo.type;

	fd = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);

	if (bind(fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		throw std::runtime_error("failed to bind l2cap server");
	}

	if (listen(fd, 5) < 0) {
		throw std::runtime_error("failed to listen l2cap server");
	}

	int fdShared = fd;
	beetle.readers.add(fd, [this, &beetle, fdShared] {
		struct sockaddr_l2 cli_addr;
		socklen_t cli_len = sizeof(cli_addr);
		int clifd = accept(fdShared, (struct sockaddr *)&cli_addr, &cli_len);
		if (clifd < 0) {
			if (debug) {
				pwarn("error on accept");
			}
			return;
		}

		if (debug) {
			pdebug("connected to " + ba2str_cpp(cli_addr.l2_bdaddr));
		}

		beetle.workers.schedule([this, clifd, cli_addr] {
			/*
			 * TODO(James): this is a hack, should be a better way to tell
			 * if it is ok to start scanning again
			 */
			startL2CAPCentralHelper(clifd, cli_addr);
		});
	});

	std::thread(&L2capServer::startLEAdvertising, this, deviceId).detach();
}

L2capServer::~L2capServer() {
	beetle.readers.remove(fd);
	shutdown(fd, SHUT_RDWR);
	close(fd);
	if (debug) {
		pdebug("l2cap server stopped");
	}
	hci_close_dev(callbackDeviceHandle);
}

void L2capServer::startL2CAPCentralHelper(int clifd, struct sockaddr_l2 cliaddr) {
	std::shared_ptr<VirtualDevice> device = NULL;
	try {
		/*
		 * Takes over the clifd
		 */
		device.reset(LEDevice::newCentral(beetle, clifd, cliaddr, [this]{
			/* Start advertising again */
			startLEAdvertisingHelper(callbackDeviceHandle);
		}));

		boost::shared_lock<boost::shared_mutex> devicesLk;
		beetle.addDevice(device, devicesLk);
		device->start();

		pdebug("connected to " + device->getName());
		if (debug) {
			pdebug(device->getName() + " has handle range [0,"
					+ std::to_string(device->getHighestHandle()) + "]");
		}
	} catch (std::exception& e) {
		pexcept(e);
		if (device) {
			beetle.removeDevice(device->getId());
		}

		/* Start advertising again */
		startLEAdvertisingHelper(callbackDeviceHandle);
	}
}

void L2capServer::setAdvertisingScanData() {
	std::string name = beetle.name;

	advertisementDataLen = 0;

	advertisementDataLen += 3;
	advertisementDataBuf[0] = 2;
	advertisementDataBuf[1] = 0x01;
	advertisementDataBuf[2] = 0x06;

	advertisementDataLen++;
	advertisementDataBuf[3] = 1 + name.length();
	advertisementDataLen++;
	advertisementDataBuf[4] = 0x09;
	advertisementDataLen += name.length();
	memcpy(advertisementDataBuf + 5, name.c_str(), name.length());

	scanDataLen = 0;

	scanDataLen++;
	scanDataBuf[0] = 1 + name.length();
	scanDataLen++;
	scanDataBuf[1] = 0x09;
	scanDataLen += name.length();
	memcpy(scanDataBuf + 2, name.c_str(), name.length());
}

bool L2capServer::startLEAdvertisingHelper(int deviceHandle) {

	// stop advertising
	if (hci_le_set_advertise_enable(deviceHandle, 0, 1000) < 0) {
		if (debug) {
			std::stringstream ss;
			ss << "failed to disable advertising: " << strerror(errno);
			pdebug(ss.str());
		}
	}

	//	set scan data
	if (hci_le_set_scan_response_data(deviceHandle, scanDataBuf, scanDataLen, 1000) != 0) {
		if (debug_advertise) {
			std::stringstream ss;
			ss << "failed to set scan response data: " << strerror(errno);
			pdebug(ss.str());
		}
	}

	// set advertisement data
	if (hci_le_set_advertising_data(deviceHandle, advertisementDataBuf, advertisementDataLen, 1000) != 0) {
		if (debug_advertise) {
			std::stringstream ss;
			ss << "failed to set advertisement data: " << strerror(errno);
			pdebug(ss.str());
		}
	}

	// set advertisement parameters, mostly to set the advertising interval to 100ms
	if (hci_le_set_advertising_parameters(deviceHandle, 1000) != 0) {
		if (debug_advertise) {
			std::stringstream ss;
			ss << "failed to set advertisement params: " << strerror(errno);
			pdebug(ss.str());
		}
	}

	// start advertising
	if (hci_le_set_advertise_enable(deviceHandle, 1, 1000) < 0) {
		if (debug_advertise) {
			std::stringstream ss;
			ss << "failed to enable advertising: " << strerror(errno);
			pdebug(ss.str());
		}
		return false;
	}

	struct hci_filter oldFilter;
	socklen_t oldFilterLen = sizeof(oldFilter);
	int result = getsockopt(deviceHandle, SOL_HCI, HCI_FILTER, &oldFilter, &oldFilterLen);
	if (result < 0) {
		pwarn("failed to save old hci filter");
	}

	struct hci_filter newFilter;
	hci_filter_clear(&newFilter);
	hci_filter_set_ptype(HCI_EVENT_PKT, &newFilter);
	hci_filter_set_event(EVT_LE_META_EVENT, &newFilter);
	result = setsockopt(deviceHandle, SOL_HCI, HCI_FILTER, &newFilter, sizeof(newFilter));
	if (result < 0) {
		pwarn("failed to set new hci filter");
	}

	return true;
}

void L2capServer::startLEAdvertising(int deviceId) {

	int deviceHandle = hci_open_dev(deviceId);
	if (deviceHandle < 0) {
		throw std::runtime_error("failed to obtain hci device handle");
	}

	// stop advertising
	if (hci_le_set_advertise_enable(deviceHandle, 0, 1000) < 0) {
		if (debug) {
			std::stringstream ss;
			ss << "failed to disable advertising: " << strerror(errno);
			pdebug(ss.str());
		}
	}

	memset(scanDataBuf, 0xFF, sizeof(scanDataBuf));
	memset(advertisementDataBuf, 0xFF, sizeof(advertisementDataBuf));

	startLEAdvertisingHelper(deviceHandle);

	uint8_t buf[256];
	while (true) {
		int n = read(deviceHandle, buf, sizeof(buf));
		if (n < 0) {
			continue;
		} else if (n < (1 + HCI_EVENT_HDR_SIZE)) {
			if (debug_scan) {
				pwarn("read less than hci evt header");
			}
			continue;
		}

		evt_le_meta_event *meta = (evt_le_meta_event *) (buf + (1 + HCI_EVENT_HDR_SIZE));
		if (debug_advertise) {
			std::stringstream ss;
			ss << "LE event: " << (int)meta->subevent;
			pdebug(ss.str());
		}

		if (meta->subevent == 0 || meta->subevent == EVT_LE_CONN_COMPLETE) {
			startLEAdvertisingHelper(deviceHandle);
		}
	}

	hci_close_dev(deviceHandle);
}


