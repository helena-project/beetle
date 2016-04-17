/*
 * ConnParams.h
 *
 *  Created on: Apr 10, 2016
 *      Author: james
 */

#ifndef INCLUDE_TCP_CONNPARAMS_H_
#define INCLUDE_TCP_CONNPARAMS_H_

/*
 * Indicate that the client is a gateway. Value is the gateway's name.
 */
const std::string TCP_PARAM_GATEWAY = "gateway";

/*
 * Indicate the device the gateway is connecting to. Value is the device id.
 */
const std::string TCP_PARAM_DEVICE = "device";

/*
 * Declare a client. Value is the name.
 */
const std::string TCP_PARAM_CLIENT = "client";

/*
 * Indicate that the client is a server. Value is a (true/false).
 * Default is false. If true, then Beetle will try to discover handles.
 */
const std::string TCP_PARAM_SERVER = "server";

#endif /* INCLUDE_TCP_CONNPARAMS_H_ */
