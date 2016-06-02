/*
 * StaticTopo.h
 *
 *  Created on: Jun 1, 2016
 *      Author: James Hong
 */

#ifndef STATICTOPO_H_
#define STATICTOPO_H_

#include <string>
#include <map>
#include <set>

#include "BeetleTypes.h"

class StaticTopo {
public:
	/*
	 * Read static mappings from file. Beetle will attempt to map
	 * these device pairs when they become available.
	 */
	StaticTopo(Beetle &beetle, std::string staticMappingsFile);
	virtual ~StaticTopo();

	/*
	 * We only want to add mappings after handles are discovered,
	 * and we know the connection has "stabilized".
	 */
	UpdateDeviceHandler getUpdateDeviceHandler();
private:
	Beetle &beetle;

	/*
	 * Map handles from KEY to devices in VALUE.
	 *
	 * Note: no need to lock since read only
	 */
	std::map<std::string, std::set<std::string>> staticMappings;
};

#endif /* STATICTOPO_H_ */
