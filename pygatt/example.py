#!/usr/bin/env python

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

from framework import ManagedSocket
from server import GattServer, ServerError
from client import GattClient, ClientError

def getArguments():
	"""
	Arguments for script.
	"""
	parser = argparse.ArgumentParser()
	parser.add_argument("--host", default="localhost", 
		help="hostname of the Beetle server")
	parser.add_argument("--port", "-p", type=int, default=3002, 
		help="port the Beetle server is runnng on")
	parser.add_argument("--debug", "-d", action='store_true', 
		help="print debugging")

	return parser.parse_args()

def printBox(s):
	""" Print a header """
	s = "|| %s ||" % s  
	print "=" * len(s)
	print s
	print "=" * len(s)

DEVICE_NAME = "Virtual HRM"

HEART_RATE_SERVICE_UUID = 0x180D
HEART_RATE_CONTROL_POINT_CHARAC_UUID = 0x2A39
HEART_RATE_MEASUREMENT_CHARAC_UUID = 0x2A37
HEART_RATE_MAX_CHARAC_UUID = 0x2A8D

BATTERY_SERVICE_UUID = 0x180F
BATTERY_LEVEL_CHARAC_UUID = 0x2A19

def setUpServer(server):
	"""Set up services and characteristic for the server."""
	server.addGapService(DEVICE_NAME)

	# TODO: Python 2.X limits assignments inside closures...
	valueDict = {
		"cntlPt" : None,
		"hrMeas" : 0,
		"hrMeasSub" : False,
		"battLvl" : 100,
		"battLvlSub" : False,
	}
	
	# Set up the heart rate service
	server.addService(HEART_RATE_SERVICE_UUID)
	cntlPtChar = server.addCharacteristic(HEART_RATE_CONTROL_POINT_CHARAC_UUID)
	def cntlPtWriteCallback(val):
 		print "Client wrote HR control point: " + " ".join("%02X" % x for x \
 			in val)
 		valueDict[cntlPt] = val
	cntlPtChar.setWriteCallback(cntlPtWriteCallback)

	hrMeasChar = server.addCharacteristic(HEART_RATE_MEASUREMENT_CHARAC_UUID, 
		allowNotify=True)
	def hrMeasReadCallback():
		return bytearray(valueDict["hrMeas"] & 0xFF)
	hrMeasChar.setReadCallback(hrMeasReadCallback)
	def hrMeasSubCallback(value):
		assert value[1] != 1
		assert value[0] == 1
		valueDict["hrMeasSub"] = True
	hrMeasChar.setSubscribeCallback(hrMeasSubCallback)
	def hrMeasUnsubCallback():
		valueDict["hrMeasSub"] = False
	hrMeasChar.setUnsubscribeCallback(hrMeasUnsubCallback)

	maxMrChar = server.addCharacteristic(HEART_RATE_MAX_CHARAC_UUID, 
		value=bytearray([0xFF]))

	# Set up the battery service
	server.addService(BATTERY_SERVICE_UUID)
	battLvlChar = server.addCharacteristic(BATTERY_LEVEL_CHARAC_UUID, 
		allowNotify=True)
	def battLvlReadCallback():
		return bytearray(valueDict["battLvl"] & 0xFF)
	battLvlChar.setReadCallback(battLvlReadCallback)
	def battLvlSubCallback(value):
		assert value[1] != 1
		assert value[0] == 1
		valueDict["battLvlSub"] = True
	battLvlChar.setSubscribeCallback(battLvlSubCallback)
	def battLvlUnsubCallback():
		valueDict["battLvlSub"] = False
	battLvlChar.setUnsubscribeCallback(battLvlUnsubCallback)

	def serverDaemon(valueDict, hrMeasChar, battLvlChar):
		"""Execute every second to trigger notifications"""
		battLvlPrev = None
		while True:
			time.sleep(1)

			currHrMeas = valueDict["hrMeas"]
			currBattLvl = valueDict["battLvl"]

			if valueDict["hrMeasSub"]:
				hrMeasChar.sendNotify(bytearray([currHrMeas & 0xFF]))
			if valueDict["battLvlSub"]:
				if battLvlPrev != currBattLvl:
					hrMeasChar.sendNotify(bytearray([currBattLvl & 0xFF]))
					battLvlPrev = valcurrBattLvl

			# increment heart rate
			valueDict["hrMeas"] = (currHrMeas + 1) % 0xFF

			# with small prob, decrement battery
			if random.random() > 0.98:
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
	services = client.discoverServices()
	for service in services:
		characs = service.discoverCharacteristics()
		for charac in characs:
			charac.discoverDescriptors()
	
	print ""
	printBox("Client")
	print client

	while True:
		pass

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
	setUpServer(server)

	s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	s = ssl.wrap_socket(s, cert_reqs=ssl.CERT_NONE)	# TODO fix this
	s.connect((args.host, args.port))

	# Send initial connection parameters. Just 0 for now.
	appParams = ["client " + DEVICE_NAME, "server true"]

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