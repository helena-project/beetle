/*
 * Auth.h
 *
 *  Created on: Apr 24, 2016
 *      Author: james
 */

#ifndef CONTROLLER_ACCESS_DYNAMICAUTH_H_
#define CONTROLLER_ACCESS_DYNAMICAUTH_H_

#include <string>

#include <controller/access/Rule.h>
#include <controller/AccessControl.h>
#include <controller/ControllerClient.h>
#include <ctime>

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

	virtual void evaluate(ControllerClient &cc, Device *server, Device *client) = 0;
protected:
	rule_t ruleId;
};

class NetworkAuth : public DynamicAuth {
public:
	NetworkAuth(rule_t r, std::string ip, bool isPrivate);
	virtual ~NetworkAuth() {};
	void evaluate(ControllerClient &cc, Device *from, Device *to);
private:
	bool isPrivate;
	std::string ip;
};

class AdminAuth : public DynamicAuth {
public:
	AdminAuth(rule_t r);
	virtual ~AdminAuth() {};
	void evaluate(ControllerClient &cc, Device *from, Device *to);
private:
	time_t expire = 0;
};

class UserAuth : public DynamicAuth {
public:
	UserAuth(rule_t r);
	virtual ~UserAuth() {};
	void evaluate(ControllerClient &cc, Device *from, Device *to);
private:
	time_t expire = 0;
};

class PasscodeAuth : public DynamicAuth {
public:
	PasscodeAuth(rule_t r);
	virtual ~PasscodeAuth() {};
	void evaluate(ControllerClient &cc, Device *from, Device *to);
private:
	time_t expire = 0;
};

#endif /* CONTROLLER_ACCESS_DYNAMICAUTH_H_ */
