/*
 * Auth.h
 *
 *  Created on: Apr 24, 2016
 *      Author: James Hong
 */

#ifndef CONTROLLER_ACCESS_DYNAMICAUTH_H_
#define CONTROLLER_ACCESS_DYNAMICAUTH_H_

#include <string>
#include <ctime>
#include <memory>

#include "BeetleTypes.h"
#include "controller/access/Rule.h"

/* Forward declarations */
class ControllerClient;

class DynamicAuth {
public:
	DynamicAuth(rule_t r);
	virtual ~DynamicAuth() {};

	enum State {
		UNATTEMPTED, IN_PROGRESS, SATISFIED, DENIED,
	};
	enum When {
		ON_MAP = 1, ON_ACCESS = 2,
	};

	State state = UNATTEMPTED;
	When when = ON_MAP;

	virtual void evaluate(std::shared_ptr<ControllerClient> cc,
			std::shared_ptr<Device> server, std::shared_ptr<Device> client) = 0;
protected:
	rule_t ruleId;
};

class NetworkAuth : public DynamicAuth {
public:
	NetworkAuth(rule_t r, std::string ip, bool isPrivate);
	virtual ~NetworkAuth() {};
	void evaluate(std::shared_ptr<ControllerClient> cc, std::shared_ptr<Device> from,
			std::shared_ptr<Device> to);
private:
	bool isPrivate;
	std::string ip;
};

class AdminAuth : public DynamicAuth {
public:
	AdminAuth(rule_t r);
	virtual ~AdminAuth() {};
	void evaluate(std::shared_ptr<ControllerClient> cc, std::shared_ptr<Device> from,
			std::shared_ptr<Device> to);
private:
	time_t expire = 0;
};

class UserAuth : public DynamicAuth {
public:
	UserAuth(rule_t r);
	virtual ~UserAuth() {};
	void evaluate(std::shared_ptr<ControllerClient> cc, std::shared_ptr<Device> from,
			std::shared_ptr<Device> to);
private:
	time_t expire = 0;
};

class PasscodeAuth : public DynamicAuth {
public:
	PasscodeAuth(rule_t r);
	virtual ~PasscodeAuth() {};
	void evaluate(std::shared_ptr<ControllerClient> cc, std::shared_ptr<Device> from,
			std::shared_ptr<Device> to);
private:
	time_t expire = 0;
};

#endif /* CONTROLLER_ACCESS_DYNAMICAUTH_H_ */
