/*
 * HciManager.h
 *
 *  Created on: Apr 24, 2016
 *      Author: james
 */

#ifndef HCI_H_
#define HCI_H_

#include <exception>
#include <string>
#include <mutex>

class HCIException : public std::exception {
  public:
	HCIException(std::string msg) : msg(msg) {};
	HCIException(const char *msg) : msg(msg) {};
    ~HCIException() throw() {};
    const char *what() const throw() { return this->msg.c_str(); };
  private:
    std::string msg;
};

class HCI {
public:
	HCI();
	virtual ~HCI();

	/*
	 * http://www.dre.vanderbilt.edu/~schmidt/android/android-4.0/external/bluetooth/bluez/tools/hcitool.c
	 *
	 *	"\t    -H, --handle <0xXXXX>  LE connection handle\n"
	 *	"\t    -m, --min <interval>   Range: 0x0006 to 0x0C80\n"
	 *	"\t    -M, --max <interval>   Range: 0x0006 to 0x0C80\n"
	 *	"\t    -l, --latency <range>  Slave latency. Range: 0x0000 to 0x03E8\n"
	 *	"\t    -t, --timeout  <time>  N * 10ms. Range: 0x000A to 0x0C80\n"
	 */
	bool setConnectionInterval(uint16_t hciHandle, uint16_t minInterval,
			uint16_t maxInterval, uint16_t latency, uint16_t supervisionTimeout,
			int to);

	/*
	 * Helper to reset HCI
	 */
	static void resetHCI();
private:
	int dd;
	std::mutex m;
};

#endif /* HCI_H_ */
