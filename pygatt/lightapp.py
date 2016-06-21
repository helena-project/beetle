#!/usr/bin/env python

"""
pygatt light app
================

This module implements a home lighting application.
"""

import argparse
import binascii
import os
import socket
import ssl
import struct
import threading
import cgi
from jinja2 import Environment, FileSystemLoader

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
RED_CHARAC_UUID = 0xFFE6
GREEN_CHARAC_UUID = 0xFFE7
BLUE_CHARAC_UUID = 0xFFE8
RGBW_CHARAC_UUID = 0xFFE9
WHITE_CHARAC_UUID = 0xFFEA

INTERNAL_REFRESH_INTERVAL = 60 * 5

class LightInstance(object):
	def __init__(self, name):
		self.name = name
		self.r = None
		self.g = None
		self.b = None
		self.w = None
		self.rgbw = None
		self.token = binascii.b2a_hex(os.urandom(8))

	@property
	def state(self):
		if not self.w:
			return "ERROR"
		elif self.w.read()[0] == 0:
			return "OFF"
		else:
			return "ON"

	@property
	def red(self):
		try:
			return self.r.read()[0]
		except Exception, err:
			print err
			return -1

	@property
	def green(self):
		try:
			return self.g.read()[0]
		except Exception, err:
			print err
			return -1

	@property
	def blue(self):
		try:
			return self.b.read()[0]
		except Exception, err:
			print err
			return -1

	@property
	def white(self):
		try:
			return self.w.read()[0]
		except Exception, err:
			print err
			return -1

	@property
	def ready(self):
		return (self.name is not None and self.r is not None and
			self.g is not None and self.b is not None and
			self.w is not None and self.rgbw is not None)

	def __str__(self):
		return self.name

def runHttpServer(port, client, reset, ready, devices):
	"""Start the HTTP server"""

	class WebServerHandler(BaseHTTPRequestHandler):
		"""Handle HTTP requests and serve a simple web UI"""

		def _serve_main(self):
			"""Serve the main page"""
			ready.wait()

			env = Environment(loader=FileSystemLoader("templates"))
			template = env.get_template("lightapp.html")

			self.send_response(200, 'OK')
			self.send_header('Content-type', 'html')
			self.end_headers()

			self.wfile.write(template.render(devices=devices))
			self.wfile.close()

		def _serve_favicon(self):
			self.send_response(200, 'OK')
			self.send_header('Content-type', 'png')
			self.end_headers()
			f = open("static/icons/lightapp.png", "rb")
			try:
				self.wfile.write(f.read())
			finally:
				f.close()
				self.wfile.close()

		def _serve_css(self):
			if self.path == "/style.css":
				f = open("static/style.css", "rb")
			elif self.path == "/fonts.css":
				f = open("static/google-fonts.css", "rb")
			else:
				self.send_error(404)
				self.end_headers()
				self.wfile.close()
				return

			self.send_response(200, 'OK')
			self.send_header('Content-type', 'css')
			self.end_headers()

			try:
				self.wfile.write(f.read())
			finally:
				f.close()
				self.wfile.close()

		WRITEABLE_FIELDS = set(["w", "r", "g", "b", "on", "off"])

		def _update_device(self):
			ctype, pdict = cgi.parse_header(
				self.headers.getheader('content-type'))

			if ctype == 'multipart/form-data':
				postvars = cgi.parse_multipart(self.rfile, pdict)
			elif ctype == 'application/x-www-form-urlencoded':
				length = int(self.headers.getheader('content-length'))
				postvars = cgi.parse_qs(self.rfile.read(length),
					keep_blank_values=1)
			else:
				self.send_error(400)
				self.end_headers()
				self.wfile.close()
				return

			if ("field" not in postvars or "value" not in postvars
				or "token" not in postvars):
				self.send_error(400)
				self.end_headers()
				self.wfile.close()
				return

			print postvars
			field = postvars["field"][0]
			if not field in self.WRITEABLE_FIELDS:
				self.send_error(400)
				self.end_headers()
				self.wfile.close()
				return

			value = -1
			if field == "on":
				value = 100
			elif field == "off":
				value = 0
			else:
				try:
					value = int(postvars["value"][0])
				except Exception, err:
					print err

			if value < 0 or value > 0xFF:
				self.send_error(400)
				self.end_headers()
				self.wfile.close()
				return

			valueToWrite = bytearray([value])

			token = postvars["token"][0]

			for device in devices:
				if device.token == token:
					if field == "w":
						device.w.write(valueToWrite)
					elif field == "r":
						device.r.write(valueToWrite)
					elif field == "g":
						device.g.write(valueToWrite)
					elif field == "b":
						device.b.write(valueToWrite)
					elif field == "off" or field == "on":
						# TODO: it would be preferable to use the RGBW char
						device.r.write(bytearray(1))
						device.g.write(bytearray(1))
						device.b.write(bytearray(1))
						device.w.write(valueToWrite)
					else:
						raise NotImplementedError

			self._serve_main()

		def do_GET(self):
			if self.path == "/":
				self._serve_main()
			elif self.path == "/favicon.ico":
				self._serve_favicon()
			elif self.path.endswith(".css"):
				self._serve_css()
			else:
				self.send_error(404)

		def do_POST(self):
			if self.path == "/rescan":
				ready.clear()
				reset.set()
				ready.wait()
				self.send_response(200, 'OK')
				self.end_headers()
				self.wfile.close()
			elif self.path == "/":
				self._update_device()
			else:
				self.send_error(404)

	server = HTTPServer(("", port), WebServerHandler)
	try:
		server.serve_forever()
	finally:
		server.socket.close()

def runClient(client, reset, ready, devices):
	"""Start a Beetle client"""

	gapUuid = uuid.UUID(gatt.GAP_SERVICE_UUID)
	nameUuid = uuid.UUID(gatt.GAP_CHARAC_DEVICE_NAME_UUID)

	lightUuid = uuid.UUID(LIGHT_SERVICE_UUID)
	redUuid = uuid.UUID(RED_CHARAC_UUID)
	greenUuid = uuid.UUID(GREEN_CHARAC_UUID)
	blueUuid = uuid.UUID(BLUE_CHARAC_UUID)
	whiteUuid = uuid.UUID(WHITE_CHARAC_UUID)
	rgbwUuid = uuid.UUID(RGBW_CHARAC_UUID)

	def _daemon():
		while True:
			del devices[:]
			services = client.discoverAll()

			currDevice = None

			# proceed down the services, separating out devices
			# TODO(james): add Beetle service to delimit devices

			printBox("Discovering handles")

			for service in services:
				print service

				if service.uuid == gapUuid:
					for charac in service.characteristics:
						print "  ", charac

						if charac.uuid == nameUuid:
							currDevice = None
							try:
								currDeviceName = str(charac.read())
								currDevice = LightInstance(name=currDeviceName)
							except ClientError, err:
								print err
							except Exception, err:
								print err

				elif service.uuid == lightUuid:
					for charac in service.characteristics:
						print "  ", charac, charac.userDescription

						if currDevice is not None:
							if charac.uuid == whiteUuid:
								currDevice.w = charac
							elif charac.uuid == redUuid:
								currDevice.r = charac
							elif charac.uuid == greenUuid:
								currDevice.g = charac
							elif charac.uuid == blueUuid:
								currDevice.b = charac
							elif charac.uuid == rgbwUuid:
								currDevice.rgbw = charac

					if currDevice is not None:
						print currDevice, currDevice.ready
					if currDevice is not None and currDevice.ready:
						devices.append(currDevice)
						currDevice = None

			ready.set()
			reset.clear()
			reset.wait(INTERNAL_REFRESH_INTERVAL)
			ready.clear()

	clientThread = threading.Thread(target=_daemon)
	clientThread.setDaemon(True)
	clientThread.start()

def main(args):
	"""Set up a web app"""

	def onDisconnect(err):
		print "Disconnect:", err
		os._exit(0)

	# Declare a managed socket, and bind a GATT client
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
