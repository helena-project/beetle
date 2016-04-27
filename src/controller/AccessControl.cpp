/*
 * AccessControl.cpp
 *
 *  Created on: Apr 18, 2016
 *      Author: james
 */

#include <controller/AccessControl.h>

#include <boost/network/protocol/http/client.hpp>
#include <boost/network/protocol/http/request.hpp>
#include <boost/thread/lock_types.hpp>
#include <cassert>
#include <exception>
#include <json/json.hpp>
#include <list>
#include <memory>
#include <sstream>

#include <ble/att.h>
#include <device/socket/tcp/TCPClientProxy.h>
#include <device/socket/tcp/TCPServerProxy.h>
#include <Device.h>
#include <controller/access/DynamicAuth.h>
#include <Handle.h>


using json = nlohmann::json;

AccessControl::AccessControl(Beetle &beetle, ControllerClient &client)
: beetle(beetle), client(client) {

}

AccessControl::~AccessControl() {

}

bool AccessControl::canMap(Device *from, Device *to) {
	std::string fromGateway;
	device_t fromId;
	switch (from->getType()) {
	case Device::BEETLE_INTERNAL:
		return true;
	case Device::IPC_APPLICATION:
	case Device::LE_PERIPHERAL:
	case Device::TCP_CONNECTION: {
		fromGateway = beetle.name;
		fromId = from->getId();
		break;
	}
	case Device::TCP_CLIENT_PROXY:
		return false;
	case Device::TCP_SERVER_PROXY: {
		TCPServerProxy *fromCast = dynamic_cast<TCPServerProxy *>(from);
		fromGateway = fromCast->getServerGateway();
		fromId = fromCast->getRemoteDeviceId();
		break;
	}
	case Device::UNKNOWN:
	default:
		return false;
	}

	std::string toGateway;
	device_t toId;
	switch (to->getType()) {
	case Device::BEETLE_INTERNAL:
		return false;
	case Device::IPC_APPLICATION:
	case Device::LE_PERIPHERAL:
	case Device::TCP_CONNECTION: {
		toGateway = beetle.name;
		toId = to->getId();
		break;
	}
	case Device::TCP_CLIENT_PROXY:
	case Device::TCP_SERVER_PROXY:
	case Device::UNKNOWN:
	default:
		return false;
	}

	std::stringstream resource;
	resource << "access/canMap/" << fromGateway << "/" << std::fixed << fromId
			<< "/" << toGateway << "/" << std::fixed << toId;

	std::string url = client.getUrl(resource.str());
	using namespace boost::network;
	http::client::request request(url);
	request << header("User-Agent", "linux");

	if (debug_controller) {
		pdebug("get: " + url);
	}
	auto response = client.getClient()->get(request);

	switch (response.status()) {
	case 200: {
		if (debug_controller) {
			pdebug("controller request ok");
		}
		try {
			std::stringstream ss;
			ss << body(response);
			return handleCanMapResponse(from, to, ss);
		} catch (std::exception &e) {
			std::stringstream ss;
			ss << "Error parsing access control server response: " << e.what();
			pwarn(ss.str());
			return false;
		}
	}
	case 500:
		pwarn("internal server error");
		return false;
	default:
		if (debug_controller) {
			std::stringstream ss;
			ss << "controller request failed " << response.status();
			pdebug(ss.str());
		}
		return false;
	}
}

RemoveDeviceHandler AccessControl::getRemoveDeviceHandler() {
	return [this](device_t d){
		boost::unique_lock<boost::shared_mutex> cacheLk(cacheMutex);
		for (auto it = cache.cbegin(); it != cache.cend();) {
			/*
			 * Remove any cached rules regarding this device.
			 */
		  if (it->first.first == d || it->first.second == d) {
			  cache.erase(it++);
		  } else {
		    ++it;
		  }
		}
	};
}

/*
 * Unpacks the controller response and returns whether the mapping is allowed.
 */
bool AccessControl::handleCanMapResponse(Device *from, Device *to, std::stringstream &response) {
	json j;
	j << response;

	if (debug_controller) {
		pdebug(json(j).dump());
	}

	bool result = j["result"];

	json access = j["access"];
	json rules = access["rules"];
	json services = access["services"];

	bool satisfiable = false;
	cached_mapping_info_t cacheEntry;
	for (json::iterator it = rules.begin(); it != rules.end(); ++it) {

		std::string ruleIdStr = it.key();
		json ruleValue = it.value();

		rule_t ruleId = std::stoi(ruleIdStr);

		Rule rule;
		rule.setProperties(ruleValue["prop"]);
		rule.encryption = ruleValue["enc"];
		rule.integrity = ruleValue["int"];
		rule.exclusive = ruleValue["excl"];
		std::string tStr = ruleValue["lease"];
		rule.lease = static_cast<time_t>(std::stod(tStr));
		rule.priority = 0;
		json dAuthValues = ruleValue["dauth"];

		if (dAuthValues.size() == 0) {
			satisfiable = true;
		} else {
			for (json::iterator it2 = dAuthValues.begin(); it2 != dAuthValues.end(); ++it2) {
				json dAuthValue = *it2;
				std::string type = dAuthValue["type"];

				DynamicAuth *auth = NULL;
				if (type == "network") {
					std::string ip = dAuthValue["ip"];
					bool isPrivate = dAuthValue["priv"];
					auth = new NetworkAuth(ruleId, ip, isPrivate);
					rule.priority += 0;
				} else if (type == "passcode") {
					auth = new PasscodeAuth(ruleId);
					rule.priority += 1 << 1;
				} else if (type == "subject") {
					rule.priority += 1 << 2;
				} else if (type == "admin") {
					rule.priority += 1 << 3;
				}

				/*
				 * Populate generic fields
				 */
				if (auth != NULL) {
					int when = dAuthValue["when"];
					auth->when = static_cast<DynamicAuth::When>(when);
				} else {
					if (debug_controller) {
						pwarn("unsupported auth type");
					}
				}

				/*
				 * Evaluate if ON_MAP
				 */
				if (auth != NULL) {
					if (auth->when == DynamicAuth::ON_MAP) {
						auth->evaluate(client, from, to);
						if (auth->state == DynamicAuth::SATISFIED) {
							// on connect rule satsfied
							satisfiable = true;
						}
					} else {
						// rule to be evaluated later
						satisfiable = true;
					}
					rule.additionalAuth.push_back(std::shared_ptr<DynamicAuth>(auth));
				} else {
					// rule with no dynamic
					satisfiable = true;
				}
			}
		}

		cacheEntry.rules[ruleId] = rule;
	}

	if (satisfiable == false) {
		/*
		 * All rules are on evalualated onMap and none are satisfiable
		 */
		if (debug_controller) {
			pdebug("no rules are satisfiable");
		}
		return false;
	}

	for (json::iterator it = services.begin(); it != services.end(); ++it) {
		std::string tmp = it.key();
		UUID serviceUuid = UUID(tmp);
		json chars = it.value();
		if (debug_controller) {
			pdebug("service: " + serviceUuid.str());
		}

		assert(cacheEntry.service_char_rules.find(serviceUuid) == cacheEntry.service_char_rules.end());
		for (json::iterator it2 = chars.begin(); it2 != chars.end(); ++it2) {
			tmp = it2.key();
			UUID charUuid = UUID(tmp);
			json ruleIds = it2.value();
			if (debug_controller) {
				pdebug("char: " + charUuid.str());
			}

			rule_info_set charRulesSet;
			for (json::iterator it3 = ruleIds.begin(); it3 != ruleIds.end(); ++it3) {
				rule_t ruleId = *it3;
				rule_info_t ruleInfo = ((uint64_t)cacheEntry.rules[ruleId].priority) << 32;
				ruleInfo |= ruleId;
				charRulesSet.insert(ruleInfo);
			}

			cacheEntry.service_char_rules[serviceUuid][charUuid] = charRulesSet;
		}
	}

	boost::unique_lock<boost::shared_mutex> lk(cacheMutex);
	cache[std::make_pair(from->getId(), to->getId())] = cacheEntry;

	return result;
}

static inline bool isReadReq(uint8_t op) {
	return op == ATT_OP_READ_BY_TYPE_REQ || op == ATT_OP_READ_REQ || op == ATT_OP_READ_BLOB_REQ
			|| op == ATT_OP_READ_MULTI_REQ || op == ATT_OP_READ_BY_GROUP_REQ;
}

static inline bool isWriteReq(uint8_t op) {
	return op == ATT_OP_WRITE_REQ || op == ATT_OP_WRITE_CMD || op == ATT_OP_PREP_WRITE_REQ
			|| op == ATT_OP_EXEC_WRITE_REQ;
}

bool AccessControl::canAccessHandle(Device *client, Device *server, Handle *handle,
		uint8_t op, uint8_t &attErr) {
	if (dynamic_cast<TCPClientProxy *>(client)) {
		return true;
	} else if (dynamic_cast<TCPServerProxy *>(client)) {
		return false;
	}

	/*
	 * Default reason to deny op.
	 */
	attErr = ATT_ECODE_ATTR_NOT_FOUND;

	boost::shared_lock<boost::shared_mutex> lk(cacheMutex);
	auto key = std::make_pair(server->getId(), client->getId());
	if (cache.find(key) == cache.end()) {
		pwarn("no cached access control rules exist for client-server");
		return false;
	}
	cached_mapping_info_t &ruleMapping = cache[key];

	/*
	 * Case 1: This handle is a service.
	 */
	PrimaryService *ps = dynamic_cast<PrimaryService *>(handle);
	if (ps) {
		if (ruleMapping.service_char_rules.find(ps->getServiceUuid()) == ruleMapping.service_char_rules.end()) {
			return false;
		} else {
			return true;
		}
	}

	/*
	 * Case 2: This handle is a characteristic.
	 */
	Characteristic *ch = dynamic_cast<Characteristic *>(handle);
	if (ch) {
		ps = dynamic_cast<PrimaryService *>(server->handles[ch->getServiceHandle()]);
		if (!ps) {
			return false;
		}
		auto serviceMap = ruleMapping.service_char_rules.find(ps->getServiceUuid());
		if (serviceMap == ruleMapping.service_char_rules.end()) {
			return false;
		}

		auto charMap = serviceMap->second.find(ch->getCharUuid());
		if (charMap == serviceMap->second.end()) {
			return false;
		} else {
			return true;
		}
	}

	/*
	 * Case 3: This is a handle that is part of a service.
	 */
	ps = dynamic_cast<PrimaryService *>(server->handles[handle->getServiceHandle()]);
	ch = dynamic_cast<Characteristic *>(server->handles[handle->getCharHandle()]);
	if (!ps || !ch) {
		return false;
	}
	auto serviceMap = ruleMapping.service_char_rules.find(ps->getServiceUuid());
	if (serviceMap == ruleMapping.service_char_rules.end()) {
		return false;
	}
	auto charMap = serviceMap->second.find(ch->getCharUuid());
	if (charMap == serviceMap->second.end()) {
		return false;
	}

	/*
	 * SubCase: char config
	 */
	ClientCharCfg *ccc = dynamic_cast<ClientCharCfg *>(handle);
	if (ccc) {
		if(!isWriteReq(op)) {
			return true;
		} else {
			for (rule_info_t rInfo : charMap->second) {
				rule_t rId = rInfo & 0xFFFFFFFF;
				Rule rule = ruleMapping.rules[rId];
				if ((rule.properties & (GATT_CHARAC_PROP_IND | GATT_CHARAC_PROP_IND)) != 0) {
					return true;
				}
			}
			attErr = ATT_ECODE_WRITE_NOT_PERM;
			return false;
		}
	}

	/*
	 * Subcase: characteristic value
	 */
	if (ch->getAttrHandle() == handle->getHandle()) {
		bool isRead = isReadReq(op);
		bool isWrite = isWriteReq(op);
		if (!isRead && !isWrite) {
			return true;
		}
		for (rule_info_t rInfo : charMap->second) {
			rule_t rId = rInfo & 0xFFFFFFFF;
			Rule rule = ruleMapping.rules[rId];
			bool satisfiable = false;
			if (isRead && (rule.properties & GATT_CHARAC_PROP_READ)) {
				satisfiable = true;
			}
			if (isWrite && (rule.properties & GATT_CHARAC_PROP_WRITE)) {
				satisfiable = true;
			}
			if (satisfiable) {
				for (std::shared_ptr<DynamicAuth> &dAuth : rule.additionalAuth) {
					if (dAuth->state == DynamicAuth::UNATTEMPTED && dAuth->when == DynamicAuth::ON_ACCESS) {
						dAuth->evaluate(this->client, server, client);
					}
					if (dAuth->state != DynamicAuth::SATISFIED) {
						satisfiable = false;
						break;
					}
				}
				if (satisfiable) {
					return true;
				}
			}
		}
		if (isRead) {
			attErr = ATT_ECODE_READ_NOT_PERM;
		}
		if (isWrite) {
			attErr = ATT_ECODE_WRITE_NOT_PERM;
		}
		return false;
	} else {
		return true;
	}
}

bool AccessControl::getCharAccessProperties(Device *client, Device *server, Handle *handle, uint8_t &properties) {
	boost::shared_lock<boost::shared_mutex> lk(cacheMutex);
	if (dynamic_cast<TCPClientProxy *>(client)) {
		properties = 0xFF;
		return true;
	} else if (dynamic_cast<TCPServerProxy *>(client)) {
		return false;
	}

	auto key = std::make_pair(server->getId(), client->getId());
	if (cache.find(key) == cache.end()) {
		pwarn("no cached access control rules exist for client-server");
		return false;
	}
	cached_mapping_info_t &ruleMapping = cache[key];
	properties = 0;

	Characteristic *ch = dynamic_cast<Characteristic *>(handle);
	if (ch) {
		PrimaryService *ps = dynamic_cast<PrimaryService *>(server->handles[ch->getServiceHandle()]);
		if (!ps) {
			return false;
		}
		auto serviceMap = ruleMapping.service_char_rules.find(ps->getServiceUuid());
		if (serviceMap == ruleMapping.service_char_rules.end()) {
			return false;
		}

		auto charMap = serviceMap->second.find(ch->getCharUuid());
		if (charMap == serviceMap->second.end()) {
			return false;
		}

		for (rule_info_t rInfo : charMap->second) {
			rule_t rId = rInfo & 0xFFFFFFFF;
			Rule rule = ruleMapping.rules[rId];
			/*
			 * Nothing to do here. This is part of discovery.
			 */
			properties |= rule.properties;
		}
		return true;
	}
	return false;
}

bool AccessControl::canReadType(Device *client, Device *server, UUID &attType) {
	if (dynamic_cast<TCPClientProxy *>(client)) {
		return true;
	} else if (dynamic_cast<TCPServerProxy *>(client)) {
		return false;
	}

	boost::shared_lock<boost::shared_mutex> lk(cacheMutex);
	auto key = std::make_pair(server->getId(), client->getId());
	if (cache.find(key) == cache.end()) {
		pwarn("no cached access control rules exist for client-server");
		return false;
	}
	cached_mapping_info_t &ruleMapping = cache[key];

	if (attType.isShort()) {
		switch (attType.getShort()) {
		case GATT_PRIM_SVC_UUID:
		case GATT_SND_SVC_UUID:
		case GATT_INCLUDE_UUID:
		case GATT_CHARAC_UUID:
		case GATT_CHARAC_EXT_PROPER_UUID:
		case GATT_CHARAC_USER_DESC_UUID:
		case GATT_CLIENT_CHARAC_CFG_UUID:
		case GATT_SERVER_CHARAC_CFG_UUID:
		case GATT_CHARAC_FMT_UUID:
		case GATT_CHARAC_AGREG_FMT_UUID:
		case GATT_CHARAC_VALID_RANGE_UUID:
		case GATT_EXTERNAL_REPORT_REFERENCE:
		case GATT_REPORT_REFERENCE:
		case GATT_GAP_SERVICE_UUID:
		case GATT_GAP_CHARAC_DEVICE_NAME_UUID:
		case GATT_GAP_CHARAC_APPEARANCE_UUID:
		case GATT_GAP_CHARAC_PERIPH_PRIV_FLAG_UUID:
		case GATT_GAP_CHARAC_PREF_CONN_PARAMS_UUID:
		case GATT_GATT_SERVICE_UUID:
			return true;
		case GATT_GAP_CHARAC_RECONNECTION_ADDR_UUID:
		case GATT_GATT_CHARAC_SERVICE_CHANGED_UUID:
			return false;
		}
	}

	for (auto &service : ruleMapping.service_char_rules) {
		auto characteristic = service.second.find(attType);
		if (characteristic != service.second.end()) {
			for (rule_info_t rInfo : characteristic->second) {
				rule_t rId = rInfo & 0xFFFFFFFF;
				Rule rule = ruleMapping.rules[rId];
				if (rule.properties && GATT_CHARAC_PROP_READ == 0) {
					continue;
				}
				bool satisfied = true;
				for (std::shared_ptr<DynamicAuth> &dAuth : rule.additionalAuth) {
					if (dAuth->state == DynamicAuth::UNATTEMPTED && dAuth->when == DynamicAuth::ON_ACCESS) {
						dAuth->evaluate(this->client, server, client);
					}
					if (dAuth->state != DynamicAuth::SATISFIED) {
						satisfied = false;
						break;
					}
				}
				if (satisfied) {
					return true;
				}
			}
		}
	}
	return false;
}
