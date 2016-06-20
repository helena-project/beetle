#!/usr/bin/env python

"""
pygatt cloud monitoring
=======================

This module implements a cloud monitoring application for home sensors
"""

import os
import socket
import ssl
import argparse
import struct

import lib.gatt as gatt
import lib.uuid as uuid
from pygatt import ManagedSocket, GattClient

def getArguments():
	"""Arguments for script."""

	parser = argparse.ArgumentParser()
	parser.add_argument("--name", default="cloudmonitor",
		help="name of the application")
	parser.add_argument("--app-port", type=int, default="8081",
		help="port to run application server")

	parser.add_argument("--host", default="localhost",
		help="hostname of the Beetle server")
	parser.add_argument("--port", "-p", type=int, default=3002,
		help="port the server is runnng on")
	parser.add_argument("--cert", "-c", type=str,
		help="client certificate")
	parser.add_argument("--key", "-k", type=str,
		help="private key for client certificate")
	parser.add_argument("--rootca", "-r",
		help="root CA certificate")

	parser.add_argument("--nocerts", "-x", action="store_true",
		help="disable verification and use of client certificates")
	return parser.parse_args()

def printBox(s):
	""" Print a header """
	print "%s\n|| %s ||\n%s" % ("=" * (len(s) + 6), s, "=" * (len(s) + 6))


ENV_SENSING_SERVICE_UUID = 0x181A
PRESSURE_CHARAC_UUID = 0x2A6D
TEMPERATURE_CHARAC_UUID = 0x2A6E
HUMIDITY_CHARAC_UUID = 0x2A6F

def runHttpServer():
	pass

def runClient(client):
	"""Start a beetle client"""

	gapUuid = uuid.UUID(gatt.GAP_SERVICE_UUID)
	nameUuid = uuid.UUID(gatt.GAP_CHARAC_DEVICE_NAME_UUID)
	envSenseUuid = uuid.UUID(ENV_SENSING_SERVICE_UUID)
	pressureUuid = uuid.UUID(PRESSURE_CHARAC_UUID)
	temperatureUuid = uuid.UUID(TEMPERATURE_CHARAC_UUID)
	humidityUuid = uuid.UUID(HUMIDITY_CHARAC_UUID)

def main(args):
	"""Set up a web app"""

	def onDisconnect(err):
		print "Disconnect:", err
		os._exit(0)

	# Declare a managed socket, and bind a GATT server and client
	managedSocket = ManagedSocket(daemon=True)
	client = GattClient(managedSocket, onDisconnect=onDisconnect)

	s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	if args.nocerts:
		s = ssl.wrap_socket(s, cert_reqs=ssl.CERT_NONE)
	else:
		s = ssl.wrap_socket(s, keyfile=args.key, certfile=args.cert,
			ca_certs=args.rootca, cert_reqs=ssl.CERT_REQUIRED)
	s.connect((args.host, args.port))

	printBox("Starting as: %s" % args.name)

	# Send initial connection parameters.
	appParams = ["client %s" % args.name, "server false"]

	print ""
	printBox("Connection request")
	for line in appParams:
		print "$ %s" % line

	# Send connection request parameters to Beetle
	appParamsStr = "\n".join(appParams)
	appParamsLength = struct.pack("!i", len(appParamsStr))
	s.sendall(appParamsLength.encode('utf-8'))
	s.sendall(appParamsStr.encode('utf-8'))

	# Read parameters in plaintext from Beetle
	serverParamsLength = struct.unpack("!i", s.recv(4))[0]

	print ""
	printBox("Beetle response")
	for serverParam in s.recv(serverParamsLength).split("\n"):
		print "$ %s" % serverParam.rstrip()

	# Transfer ownership of the socket
	managedSocket.bind(s, True)

	devices = []
	reset = threading.Event()
	ready = threading.Event()

	runClient(client, reset, ready, devices)
	runHttpServer(args.app_port, client, reset, ready, devices)

if __name__ == "__main__":
	args = getArguments()
	main(args)
	os._exit(0)
