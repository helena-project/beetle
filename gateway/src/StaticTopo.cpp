/*
 * StaticTopo.cpp
 *
 *  Created on: Jun 1, 2016
 *      Author: James Hong
 */

#include "StaticTopo.h"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/thread/lock_types.hpp>
#include <boost/thread/pthread/shared_mutex.hpp>
#include <boost/token_functions.hpp>
#include <boost/tokenizer.hpp>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <memory>
#include <sstream>
#include <utility>

#include "Beetle.h"
#include "Debug.h"
#include "Device.h"
#include "util/file.h"
#include "util/trim.h"


StaticTopo::StaticTopo(Beetle &beetle, std::string staticMappingsFile) :
		beetle(beetle) {
	if (!file_exists(staticMappingsFile)) {
		throw std::invalid_argument("static mappings file does not exist - " + staticMappingsFile);
	}

	std::cout << "static topology file: " << staticMappingsFile << std::endl;

	std::ifstream ifs(staticMappingsFile);
	std::string line;
	while (std::getline(ifs, line)) {
		std::string mapping = line;
		if (mapping.find('#') != mapping.npos) {
			mapping = mapping.substr(0, mapping.find('#'));
		}

		trim(mapping);
		if (mapping == "") {
			continue;
		}

		// heavy handed way
		boost::char_separator<char> sep { "\t" };
		boost::tokenizer<boost::char_separator<char>> tokenizer { mapping, sep };

		std::string from;
		std::string to;
		int i = 0;
		for (const auto &t : tokenizer) {
			switch (i) {
			case 0:
				from = trimmed(t);
				break;
			case 1:
				to = trimmed(t);
				break;
			default:
				throw ParseExecption("extraneous token - " + t);
			}
			i++;
		}

		if (i < 2) {
			throw ParseExecption("could not parse - " + line);
		}

		if (from == to) {
			throw ParseExecption("cannot map device to itself! - " + line);
		}

		std::cout << "  " << from << " -> " << to << std::endl;
		staticMappings[from].insert(to);
	}
}

StaticTopo::~StaticTopo() {

}

UpdateDeviceHandler StaticTopo::getUpdateDeviceHandler() {
	return [this](device_t id) {
		boost::shared_lock<boost::shared_mutex> devicesLk(beetle.devicesMutex);
		if (beetle.devices.find(id) == beetle.devices.end()) {
			if (debug) {
				std::stringstream ss;
				ss << id << "does not id a device";
				pdebug(ss.str());
			}
			return;
		}

		std::string deviceName = beetle.devices[id]->getName();
		auto thisEntry = staticMappings.find(deviceName);

		if (debug_topology) {
			std::stringstream ss;
			ss << "setting up mappings for " << deviceName;
			pdebug(ss.str());
		}

		std::set<device_t> mapFromIds;
		std::set<device_t> mapToIds;
		for (auto &kv : beetle.devices) {
			if (kv.first == id) {
				continue;
			}

			std::string otherName = kv.second->getName();

			auto otherEntry = staticMappings.find(otherName);
			if (otherEntry != staticMappings.end() &&
					otherEntry->second.find(deviceName) != otherEntry->second.end()) {
				mapFromIds.insert(kv.first);
			}

			if (thisEntry != staticMappings.end() &&
					thisEntry->second.find(otherName) != thisEntry->second.end()) {
				mapToIds.insert(kv.first);
			}
		}
		devicesLk.unlock();

		for (device_t to : mapToIds) {
			beetle.mapDevices(id, to);
		}

		for (device_t from : mapFromIds) {
			beetle.mapDevices(from, id);
		}
	};
}
