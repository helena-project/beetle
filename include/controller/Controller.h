/*
 * ControllerException.h
 *
 *  Created on: Apr 17, 2016
 *      Author: james
 */

#ifndef CONTROLLER_CONTROLLER_H_
#define CONTROLLER_CONTROLLER_H_

#include <exception>
#include <sstream>
#include <string>

class NetworkException : public std::exception {
  public:
	NetworkException(std::string msg) : msg(msg) {};
	NetworkException(const char *msg) : msg(msg) {};
    ~NetworkException() throw() {};
    const char *what() const throw() { return this->msg.c_str(); };
  private:
    std::string msg;
};

inline std::string getUrl(std::string hostAndPort, std::string resource) {
	std::stringstream ss;
	ss << "http://" << hostAndPort << "/" << resource;
	return ss.str();
}

#endif /* CONTROLLER_CONTROLLER_H_ */
