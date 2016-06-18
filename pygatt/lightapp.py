#!/usr/bin/env python

"""
pygatt light app
================

This module implements a home lighting application.
"""

import os
import socket
import ssl
import argparse
import struct
import traceback
import threading
from recordclass import recordclass

from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer

import lib.gatt as gatt
import lib.uuid as uuid
from pygatt import ManagedSocket, GattClient, ClientError

def getArguments():
	"""Arguments for script."""

	parser = argparse.ArgumentParser()
	parser.add_argument("--name", default="lightapp",
		help="name of the application")
	parser.add_argument("--app-port", type=int, default="8080",
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

LIGHT_SERVICE_UUID = 0xFFE5
# RED_CHARAC_UUID =
# GREEN_CHARAC_UUID =
# BLUE_CHARAC_UUID =
# RGBW_CHARAC_UUID = 0xFFE9
WHITE_CHARAC_UUID = 0xFFEB

LightInstance = recordclass("LightInstance", "name w")

def runHttpServer(port, client, reset, devices):
	"""Start the HTTP server"""

	class WebServerHandler(BaseHTTPRequestHandler):
		"""Handle HTTP requests and serve a simple web UI"""

		def do_GET(self):
			pass

		def do_POST(self):
			pass

	server = HTTPServer(("", port), WebServerHandler)
	try:
		server.serve_forever()
	finally:
		server.socket.close()

def runClient(client, reset, devices):
	"""Start a Beetle client"""

	nameUuid = uuid.UUID(gatt.GAP_CHARAC_DEVICE_NAME_UUID)
	lightUuid = uuid.UUID(LIGHT_SERVICE_UUID)
	whiteUuid = uuid.UUID(WHITE_CHARAC_UUID)

	def _daemon():
		while True:
			devices.clear()
			services = client.discoverAll()

			currDevice = None

			# proceed down the services, separating out devices
			# TODO(james): add Beetle service to delimit devices
			for service in services:
				if service.uuid == uuid.UUID(gatt.GAP_SERVICE_UUID):
					for charac in service.characteristics:
						if charac.uuid == nameUuid:
							if currDevice and currDevice.w is not None:
								devices.append(currDevice)
								currDevice = None

							try:
								currDeviceName = str(charac.read())
								currDevice = LightInstance(
									name=currDeviceName,
									w=None)
							except ClientError, err:
								print err
							except Exception, err:
								print err

				elif service.uuid == lightUuid:
					for charac in service.characteristics:
						if charac.uuid == whiteUuid:
							if currDevice is not None:
								currDevice.w = charac

			if currDevice is not None:
				devices.append(currDevice)

			reset.wait()

	clientThread = threading.Thread(target=_daemon)
	clientThread.setDaemon(True)
	clientThread.start()

def main(args):
	"""Set up and run an example HRM server and client"""

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

	printBox("Starting as %s" % args.name)

	# Send initial connection parameters.
	appParams = ["client %s" % args.name, "server true"]

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

	runClient(client, reset, devices)
	runHttpServer(args.app_port, client, reset, devices)

if __name__ == "__main__":
	try:
		args = getArguments()
		main(args)
	except KeyboardInterrupt, err:
		traceback.print_exc()
		os._exit(0)
