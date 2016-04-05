/*
 * Discover.cpp
 *
 *  Created on: Mar 24, 2016
 *      Author: james
 */

#include "Scanner.h"

#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <map>
#include <sstream>

#include "ble/helper.h"
#include "Debug.h"

#define EIR_NAME_SHORT		0x08
#define EIR_NAME_COMPLETE  	0x09

Scanner::Scanner() : t() {
	stopped = false;
	started = false;
}

Scanner::~Scanner() {
	stopped = true;
	if (t.joinable()) t.join();
}

void Scanner::start() {
	assert(started == false);
	started = true;
	t = std::thread(&Scanner::discoverDaemon, this);
}

void Scanner::stop() {
	stopped = true;
}

std::map<std::string, peripheral_info_t> Scanner::getDiscovered() {
	std::lock_guard<std::mutex> lg(discoveredMutex);
	std::map<std::string, peripheral_info_t>  tmp = discovered;
	discovered = std::map<std::string, peripheral_info_t>();
	return tmp;
}

// TODO check return values for calls
void Scanner::discoverDaemon() {
	if (debug) {
		pdebug("discoverDaemon started");
	}

 	int deviceId = hci_get_route(NULL);
 	assert(deviceId >= 0);
 	int deviceHandle = hci_open_dev(deviceId);
 	assert(deviceHandle >= 0);

 	hci_le_set_scan_parameters(deviceHandle, 0x01, htobs(0x0010),
 			htobs(0x0010), 0x00, 0x00, 1000);
 	hci_le_set_scan_enable(deviceHandle, 0x01, 1, 1000);

	struct hci_filter oldFilter = {0};
 	socklen_t oldFilterLen = sizeof(oldFilter);
  	getsockopt(deviceHandle, SOL_HCI, HCI_FILTER, &oldFilter, &oldFilterLen);

  	struct hci_filter newFilter;
	hci_filter_clear(&newFilter);
  	hci_filter_set_ptype(HCI_EVENT_PKT, &newFilter);
  	hci_filter_set_event(EVT_LE_META_EVENT, &newFilter);
  	setsockopt(deviceHandle, SOL_HCI, HCI_FILTER, &newFilter, sizeof(newFilter));

  	uint8_t buf[HCI_MAX_EVENT_SIZE];
  	while (!stopped) {
  		int n = read(deviceHandle, buf, sizeof(buf));
  		if (n < (1 + HCI_EVENT_HDR_SIZE)) {
  			continue;
  		}

  		evt_le_meta_event *meta = (evt_le_meta_event *)(buf + (1 + HCI_EVENT_HDR_SIZE));
  		if (meta->subevent != EVT_LE_ADVERTISING_REPORT) {
  			continue;
  		}
  		le_advertising_info *info = (le_advertising_info *)(meta->data + 1);

  		std::string addr = ba2str_cpp(info->bdaddr);
  		AddrType addrType = (info->bdaddr_type == LE_PUBLIC_ADDRESS) ? PUBLIC : RANDOM;
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
  			ss << "discovered " << addr << "\t" << ((addrType == PUBLIC) ? "public" : "random")
  					<< "\t" << name;
  			pdebug(ss.str());
  		}

  		std::lock_guard<std::mutex> lg(discoveredMutex);
  		if (discovered.find(addr) != discovered.end() && discovered[addr].name.length() != 0) {
  			continue; // didn't get anything new
  		} else {
  			discovered[addr] = peripheral_info_t{name, info->bdaddr, addrType};
  		}
  	}

  	// teardown
  	assert(hci_le_set_scan_enable(deviceHandle, 0x00, 1, 1000) >= 0);
  	assert(setsockopt(deviceHandle, SOL_HCI, HCI_FILTER, &oldFilter, sizeof(oldFilter)) >= 0);
  	hci_close_dev(deviceHandle);

	if (debug) {
		pdebug("discoverDaemon exited");
	}
}
