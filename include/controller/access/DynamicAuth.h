/*
 * Auth.h
 *
 *  Created on: Apr 24, 2016
 *      Author: james
 */

#ifndef CONTROLLER_ACCESS_DYNAMICAUTH_H_
#define CONTROLLER_ACCESS_DYNAMICAUTH_H_

#include <string>

#include <Device.h>

class DynamicAuth {
public:
	DynamicAuth();
	virtual ~DynamicAuth();

	enum State {
		UNATTEMPTED, SATISFIED, DENIED,
	};
	enum When {
		ON_MAP = 1, ON_ACCESS = 2,
	};

	State state = UNATTEMPTED;
	When when = ON_MAP;

	virtual void evaluate(Device *server, Device *client) = 0;
};

class NetworkAuth : public DynamicAuth {
public:
	bool isPrivate;
	std::string ip;
	void evaluate(Device *from, Device *to);
};

//class AdminAuth : public DynamicAuth {
//	// TODO
//};
//
//class SubjectAuth : public DynamicAuth {
//	// TODO
//};
//
//class PasscodeAuth : public DynamicAuth {
//	// TODO
//};

#endif /* CONTROLLER_ACCESS_DYNAMICAUTH_H_ */
