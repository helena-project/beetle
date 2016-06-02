#!/usr/bin/env python

"""
pygatt example
==============

This module implements a virtual heart rate monitor server and a client
that performs discovery.
"""


import sys
import os
import socket
import ssl
import argparse
import threading
import time
import random
import struct
import traceback

import lib.gatt as gatt
import lib.att as att
import lib.uuid as uuid

from pygatt import *

def getArguments():
	"""
	Arguments for script.
	"""
	parser = argparse.ArgumentParser()
	parser.add_argument("--host", default="localhost", 
		help="hostname of the Beetle server")
	parser.add_argument("--port", "-p", type=int, default=3002, 
		help="port the Beetle server is runnng on")
	parser.add_argument("--name", "-n", type=str, default="Virtual HRM", 
		help="port the Beetle server is runnng on")

	return parser.parse_args()

def printBox(s):
	""" Print a header """
	s = "|| %s ||" % s  
	print "=" * len(s)
	print s
	print "=" * len(s)

HEART_RATE_SERVICE_UUID = 0x180D
HEART_RATE_CONTROL_POINT_CHARAC_UUID = 0x2A39
HEART_RATE_MEASUREMENT_CHARAC_UUID = 0x2A37
HEART_RATE_MAX_CHARAC_UUID = 0x2A8D

BATTERY_SERVICE_UUID = 0x180F
BATTERY_LEVEL_CHARAC_UUID = 0x2A19

def setUpServer(server, deviceName):
	"""Set up services and characteristic for the server."""
	server.addGapService(deviceName)

	# TODO: Python 2.X limits assignments inside closures...
	valueDict = {
		"cntlPt" : None,
		"hrMeas" : 0,
		"hrMeasSubNotify" : False,
		"battLvl" : 100,
		"battLvlSubIndicate" : False,
	}
	
	# Set up the heart rate service
	server.addService(HEART_RATE_SERVICE_UUID)
	cntlPtChar = server.addCharacteristic(HEART_RATE_CONTROL_POINT_CHARAC_UUID)
	def cntlPtWriteCallback(val):
 		print "Client wrote HR control point: " + " ".join("%02X" % x for x \
 			in val)
 		valueDict[cntlPt] = val
	cntlPtChar.setWriteCallback(cntlPtWriteCallback)

	# Demonstrate notifications
	hrMeasChar = server.addCharacteristic(HEART_RATE_MEASUREMENT_CHARAC_UUID, 
		allowNotify=True)
	def hrMeasReadCallback():
		# Be extra careful with bytearray(1) vs bytearray([...])
		ret = bytearray(1)
		ret[0] = valueDict["hrMeas"] & 0xFF
		return ret
	hrMeasChar.setReadCallback(hrMeasReadCallback)
	def hrMeasSubCallback(value):
		if value[0] == 1:
			valueDict["hrMeasSubNotify"] = True
	hrMeasChar.setSubscribeCallback(hrMeasSubCallback)
	def hrMeasUnsubCallback():
		valueDict["hrMeasSubNotify"] = False
	hrMeasChar.setUnsubscribeCallback(hrMeasUnsubCallback)

	maxHrChar = server.addCharacteristic(HEART_RATE_MAX_CHARAC_UUID, 
		value=bytearray([0xFF]))

	# Set up the battery service
	server.addService(BATTERY_SERVICE_UUID)

	# Demonstrate indications
	battLvlChar = server.addCharacteristic(BATTERY_LEVEL_CHARAC_UUID, 
		allowIndicate=True)
	def battLvlReadCallback():
		# Be extra careful with bytearray(1) vs bytearray([...])
		ret = bytearray(1)
		ret[0] = valueDict["battLvl"] & 0xFF
		return ret
	battLvlChar.setReadCallback(battLvlReadCallback)
	def battLvlSubCallback(value):
		if value[1] == 1:
			valueDict["battLvlSubIndicate"] = True
	battLvlChar.setSubscribeCallback(battLvlSubCallback)
	def battLvlUnsubCallback():
		valueDict["battLvlSubIndicate"] = False
	battLvlChar.setUnsubscribeCallback(battLvlUnsubCallback)

	def serverDaemon(valueDict, hrMeasChar, battLvlChar):
		"""Execute every second to trigger notifications"""
		def battIndicateCallback():
			print "Confirm", battLvlChar.uuid

		battLvlPrev = None
		while True:
			time.sleep(1)

			currHrMeas = valueDict["hrMeas"]
			currBattLvl = valueDict["battLvl"]

			if valueDict["hrMeasSubNotify"]:
				hrMeasChar.sendNotify(bytearray([currHrMeas & 0xFF]))

			if valueDict["battLvlSubIndicate"]:
				if battLvlPrev != currBattLvl:
					battLvlChar.sendIndicate(bytearray([currBattLvl & 0xFF]), 
						battIndicateCallback)
					battLvlPrev = currBattLvl

			# increment heart rate
			valueDict["hrMeas"] = (currHrMeas + 1) % 0xFF

			# with small prob, decrement battery
			if random.random() > 0.9:
				currBattLvl = (currBattLvl - 1) if currBattLvl > 0 else 100
				valueDict["battLvl"] = currBattLvl

	hrMeasThread = threading.Thread(target=serverDaemon, 
		args=(valueDict, hrMeasChar, battLvlChar))
	hrMeasThread.setDaemon(True)
	hrMeasThread.start()

	printBox("Server")
	print server

def runClient(client):
	"""Begin issuing requests as a client"""

	while True:
		print ""
		raw_input("Press ENTER to begin discovery:")

		# discovery example
		services = client.discoverServices()
		for service in services:
			characteristics = service.discoverCharacteristics()
			for characteristic in characteristics:
				characteristic.discoverDescriptors()

		printBox("Client")
		print client

		# read example
		print "\nIssuing reads:"
		for service in client.services:
			for characteristic in service.characteristics:
				if "r" in characteristic.permissions:
					try:
						charVal = characteristic.read()
						print "Read: " + " ".join("%02x" % x for x in charVal)
					except ClientError, err:
						print "Caught exception:", err

		# subscription example
		print "\nSubscribing to notifications and indications:"
		for service in client.services:
			for characteristic in service.characteristics:
				if ("n" in characteristic.permissions or 
					"i" in characteristic.permissions):

					def makeCallback(characteristic):
						# This captures characteristic
						def _callback(value):
							print "Data from", characteristic.uuid
							print ">>>", " ".join("%02x" % x for x in value)
						return _callback
					
					characteristic.subscribe(makeCallback(characteristic))
					print "Subscribed:", characteristic

def main(args):
	"""Set up and run an example HRM server and client"""

	def onDisconnect(err):
		print "Disconnect:", err
		os._exit(0)

	# Declare a managed socket, and bind a GATT server and client
	managedSocket = ManagedSocket(daemon=True)
	server = GattServer(managedSocket, onDisconnect=onDisconnect)
	client = GattClient(managedSocket)

	# Setup the services and characteristics
	setUpServer(server, args.name)

	s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	s = ssl.wrap_socket(s, cert_reqs=ssl.CERT_NONE)	# TODO fix this
	s.connect((args.host, args.port))

	# Send initial connection parameters.
	appParams = ["client " + args.name, "server true"]

	print ""
	printBox("Connection request")
	for line in appParams:
		print "$ %s" % line

	# Send connection request parameters to Beetle
	appParamsStr = "\n".join(appParams)
	appParamsLength = struct.pack("!i", len(appParamsStr))
	s.send(appParamsLength.encode('utf-8'))
	s.send(appParamsStr.encode('utf-8'))

	# Read parameters in plaintext from Beetle
	serverParamsLength = struct.unpack("!i", s.recv(4))[0]
	
	print ""
	printBox("Beetle response")
	for serverParam in s.recv(serverParamsLength).split("\n"):
		print "$ %s" % serverParam.rstrip()

	# Transfer ownership of the socket
	managedSocket.bind(s, True)

	runClient(client);

if __name__ == "__main__":
	try:
		args = getArguments()
		main(args)
	except KeyboardInterrupt, err:
		traceback.print_exc()
		os._exit(0)