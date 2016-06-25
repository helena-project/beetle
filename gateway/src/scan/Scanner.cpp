/*
 * Scanner.cpp
 *
 *  Created on: Mar 24, 2016
 *      Author: James Hong
 */

#include "scan/Scanner.h"

#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <set>
#include <sstream>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#include "ble/utils.h"
#include "Debug.h"
#include "HCI.h"

#define EIR_NAME_SHORT		0x08
#define EIR_NAME_COMPLETE  	0x09

/*
 * Not very clean, but only way to clean up HCI sockets.
 */
static std::atomic_flag staticInitDone;
static std::set<int> staticDeviceHandles;

Scanner::Scanner(std::string device) {
	/*
	 * Setup static variables exactly once.
	 */
	if (!staticInitDone.test_and_set()) {
		std::atexit([]{
			for (int dd : staticDeviceHandles) {
				if (debug_scan) {
					pdebug("stopping scan and closing device");
				}
				hci_close_dev(dd);
			}
		});
	}

	deviceId = hci_devid(device.c_str());
	if (deviceId < 0) {
		throw ScannerException("could not get hci device");
	}
	std::cout << "scanning on: " << device << std::endl;

	/*
	 * Set these intervals high since we are filtering duplicates manually.
	 */
	scanInterval = 0x0100;	// TODO these should be configurable
	scanWindow = 0x0010;

	running.reset(new std::atomic_flag(true));
}

Scanner::~Scanner() {
	running->clear();

	/*
	 * Can't join daemon since shutdown(SHUR_RDWR) doesn't work on hci sockets.
	 * Have to destruct without waiting.
	 */
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
		pwarn("failed to set scan enable");
	}

	struct hci_filter oldFilter;
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

static void scanDaemon(std::shared_ptr<std::atomic_flag> running, std::vector<DiscoveryHandler> handlers,
		int deviceId, uint16_t scanInterval, uint16_t scanWindow) {
	if (debug) {
		pdebug("scanDaemon started");
	}

	int deviceHandle = hci_open_dev(deviceId);
	if (deviceHandle < 0) {
		throw ScannerException("could not get handle to hci device");
	} else {
		staticDeviceHandles.insert(deviceHandle);
	}

 	startScanHelper(deviceHandle, scanInterval, scanWindow);

	uint8_t buf[HCI_MAX_EVENT_SIZE];
	while (running->test_and_set()) {
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
		switch (meta->subevent) {
		case EVT_LE_ADVERTISING_REPORT: {
			le_advertising_info *info = (le_advertising_info *) (meta->data + 1);

			std::string addr = ba2str_cpp(info->bdaddr);
			LEDevice::AddrType addrType = (info->bdaddr_type == LE_PUBLIC_ADDRESS) ?
					LEDevice::AddrType::PUBLIC : LEDevice::AddrType::RANDOM;
			std::string name = "";
			int i = 0;
			while (i < info->length) {
				int len = ((uint8_t *)(info->data))[i]; // Hack to suppress warning ...
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
						<< ((addrType == LEDevice::AddrType::PUBLIC) ? "public" : "random")
						<< "\t" << name;
				pdebug(ss.str());
			}

			for (auto &cb : handlers) {
				bool isRunning = running->test_and_set();
				if (isRunning) {
					/*
					 * Not quite memory safe on exit
					 */
					cb(peripheral_info_t { name, info->bdaddr, addrType });
				} else {
					running->clear();
					break;
				}
			}
			break;
		}
		case EVT_LE_CONN_COMPLETE:	// start scanning again
			startScanHelper(deviceHandle, scanInterval, scanWindow);
			break;
		}
	}

	hci_close_dev(deviceHandle);

	if (debug) {
		pdebug("scanDaemon exited");
	}
}

void Scanner::start() {
	std::thread t = std::thread(&scanDaemon, running, handlers, deviceId, scanInterval, scanWindow);
	t.detach();
}

void Scanner::registerHandler(DiscoveryHandler handler) {
	handlers.push_back(handler);
}
