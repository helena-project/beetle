/*
 * CLI.cpp
 *
 *  Created on: Mar 28, 2016
 *      Author: james
 */

#include "CLI.h"

#include <boost/token_functions.hpp>
#include <boost/tokenizer.hpp>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <iterator>

CLI::CLI(Beetle &beetle) {
	CLI::beetle = beetle;
	t(cmdLineDaemon);
}

CLI::~CLI() {
	// TODO Auto-generated destructor stub
}

void CLI::join() {
	assert(t.joinable());
	t.join();
}

void CLI::cmdLineDaemon() {
	while (true) {
		std::vector<std::string> cmd;
		if (!getCommand(cmd)) return;
		if (cmd.size() == 0) continue;

		std::string c1 = cmd[0];
		if (c1 == "set-interval") {

		} else if (c1 == "discover") {

		} else if (c1 == "connect") {

		} else if (c1 == "disconnect") {

		} else if (c1 == "start") {

		} else if (c1 == "devices") {

		} else if (c1 == "handles") {

		} else if (c1 == "debug") {

		}
	}
}

bool CLI::getCommand(std::vector<std::string> &ret) {
	assert(ret.empty());
	std::cout << "> ";
	std::string line;
	getline(std::cin, line);
	transform(line.begin(), line.end(), line.begin(), ::tolower);
	if (std::cin.eof()) {
		std::cout << "Exiting..." << std::endl;
		return false;
	} else if (std::cin.bad()) {
		throw "Error";
	} else {
		boost::char_separator<char> sep{" "};
		boost::tokenizer<boost::char_separator<char>> tokenizer{line, sep};
		for (const auto &t : tokenizer) {
			ret.push_back(t);
		}
		return true;
	}
}

