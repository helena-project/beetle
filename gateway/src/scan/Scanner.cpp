/*
 * Scanner.cpp
 *
 *  Created on: Mar 24, 2016
 *      Author: James Hong
 */

#include "scan/Scanner.h"

#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cassert>
#include <cstring>
#include <sstream>

#include "ble/helper.h"
#include "Debug.h"

#define EIR_NAME_SHORT		0x08
#define EIR_NAME_COMPLETE  	0x09

Scanner::Scanner() : daemonThread() {
	daemonRunning = false;

	/*
	 * Set these intervals high since we are filtering duplicates manually.
	 */
 	scanInterval = 0x0100;	// TODO these should be configurable
 	scanWindow = 0x0010;

 	int deviceId = hci_get_route(NULL);
 	if (deviceId < 0) {
 		throw ScannerException("could not get hci device");
 	}

 	deviceHandle = hci_open_dev(deviceId);
 	if (deviceHandle < 0) {
 		throw ScannerException("could not get handle to hci device");
 	}
}

Scanner::~Scanner() {
	daemonRunning = false;
	shutdown(deviceHandle, SHUT_RDWR);

	if (daemonThread.joinable()) {
		daemonThread.join();
	}

	hci_close_dev(deviceHandle);
}

void Scanner::start() {
	assert(daemonRunning == false);
	daemonRunning = true;
	daemonThread = std::thread(&Scanner::scanDaemon, this);
}

void Scanner::registerHandler(DiscoveryHandler handler) {
	handlers.push_back(handler);
}

static struct hci_filter startScanHelper(int deviceHandle, uint16_t scanInterval,
		uint16_t scanWindow) {
 	int result = hci_le_set_scan_parameters(deviceHandle,
			0,						// type (0 is passive)
			htobs(scanInterval),	// interval
 			htobs(scanWindow), 		// window
			0, 						// own_type
			0, 						// filter
			1000);					// to (for timeout)
 	if (result < 0) {
 		pwarn("failed to set LE scan parameters");
 	}

 	result = hci_le_set_scan_enable(deviceHandle,
 			0x01,		// enable
			0, 			// do not filter duplicates
			1000);		// to (see above)
 	if (result < 0) {
 		pwarn("warning: failed to set scan enable");
 	}

 	struct hci_filter oldFilter = {0};
 	socklen_t oldFilterLen = sizeof(oldFilter);
 	result = getsockopt(deviceHandle, SOL_HCI, HCI_FILTER, &oldFilter, &oldFilterLen);
 	if (result < 0) {
 		throw ScannerException("failed to save old hci filter");
 	}

 	struct hci_filter newFilter;
 	hci_filter_clear(&newFilter);
 	hci_filter_set_ptype(HCI_EVENT_PKT, &newFilter);
 	hci_filter_set_event(EVT_LE_META_EVENT, &newFilter);
 	result = setsockopt(deviceHandle, SOL_HCI, HCI_FILTER, &newFilter, sizeof(newFilter));
 	if (result < 0) {
 		throw ScannerException("failed to set new hci filter");
 	}

 	return oldFilter;
}

//static void stopScanHelper(int deviceHandle, struct hci_filter oldFilter) {
//	int result = hci_le_set_scan_enable(deviceHandle,
//			0, 		// disable
//			1, 		// filter_dup
//			1000);	// to
//	if (result < 0) {
//		throw ScannerException("failed to disable scan");
//	}
//
//	result = setsockopt(deviceHandle, SOL_HCI, HCI_FILTER, &oldFilter, sizeof(oldFilter));
//	if (result < 0) {
//		throw ScannerException("failed to replace old hci filter");
//	}
//}

void Scanner::scanDaemon() {
	if (debug) {
		pdebug("scanDaemon started");
	}

// 	struct hci_filter oldFilter = startScanHelper(deviceHandle, scanInterval, scanWindow);
	startScanHelper(deviceHandle, scanInterval, scanWindow);

  	uint8_t buf[HCI_MAX_EVENT_SIZE];
  	while (daemonRunning) {
  		int n = read(deviceHandle, buf, sizeof(buf));
  		if (n < 0) {
  			continue;
  		} else if (n < (1 + HCI_EVENT_HDR_SIZE)) {
  			continue;
  		}

  		evt_le_meta_event *meta = (evt_le_meta_event *)(buf + (1 + HCI_EVENT_HDR_SIZE));
  		switch (meta->subevent) {
  		case EVT_LE_ADVERTISING_REPORT:
  		{
			le_advertising_info *info = (le_advertising_info *)(meta->data + 1);

			std::string addr = ba2str_cpp(info->bdaddr);
			LEPeripheral::AddrType addrType = (info->bdaddr_type == LE_PUBLIC_ADDRESS) ?
					LEPeripheral::AddrType::PUBLIC : LEPeripheral::AddrType::RANDOM;
			std::string name = "";
			int i = 0;
			while (i < info->length) {
				int len = info->data[i];
				if (info->data[i + 1] == EIR_NAME_SHORT || info->data[i + 1] == EIR_NAME_COMPLETE) {
					char name_c_str[len];
					name_c_str[len - 1] = '\0';
					memcpy(name_c_str, info->data + i + 2, len - 1);
					name = std::string(name_c_str);
				}
				i += len + 1;
			}

			if (debug_scan) {
				std::stringstream ss;
				ss << "advertisement for " << addr << "\t"
						<< ((addrType == LEPeripheral::AddrType::PUBLIC) ? "public" : "random")
						<< "\t" << name;
				pdebug(ss.str());
			}

			for (auto &cb : handlers) {
				cb(addr, peripheral_info_t{name, info->bdaddr, addrType});
			}
			break;
  		}
  		case EVT_LE_CONN_COMPLETE:	// start scanning again
  			startScanHelper(deviceHandle, scanInterval, scanWindow);
  			break;
  		}
  	}

  	/*
  	 * Since RW is shutdown, the following line does nothing.
  	 */
//  	stopScanHelper(deviceHandle, oldFilter);

	if (debug) {
		pdebug("scanDaemon exited");
	}
}
