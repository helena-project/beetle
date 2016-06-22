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
import threading
from datetime import datetime
from jinja2 import Environment, FileSystemLoader

from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer

import lib.gatt as gatt
import lib.uuid as uuid
import lib.beetle as beetle
from pygatt import ManagedSocket, GattClient, ClientError

def getArguments():
	"""Arguments for script."""

	parser = argparse.ArgumentParser()
	parser.add_argument("--name", default="sensorapp",
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
UNK1_CHARAC_UUID = 0xC512
UNK2_CHARAC_UUID = 0xF801

INTERNAL_REFRESH_INTERVAL = 60 * 5

class SensorInstance(object):
	def __init__(self, name):
		self.name = name
		self.address = None
		self.connectTime = None
		self._pressure = None
		self._pressure_cached = None
		self._temperature = None
		self._temperature_cached = None
		self._humidity = None
		self._humidity_cached = None
		self._unk1 = None
		self._unk1_cached = None
		self._unk2 = None
		self._unk2_cached = None

	def _unpack_pressure(self, buf):
		if len(buf) != 4:
			return float("nan")
		raw = struct.unpack('<I', bytes(buf))[0]
		return float(raw) / 10.0

	@property
	def pressure(self):
		if self._pressure_cached is not None:
			return self._pressure_cached

		try:
			buf = self._pressure.read()
			self._pressure_cached = self._unpack_pressure(buf)
			return self._pressure_cached

		except Exception, err:
			print err

		return float("nan")

	def _unpack_temperature(self, buf):
		if len(buf) != 2:
			return float("nan")
		raw = struct.unpack('<h', bytes(buf))[0]
		return float(raw) / 100.0

	@property
	def temperature(self):
		if self._temperature_cached is not None:
			return self._temperature_cached

		try:
			buf = self._temperature.read()
			self._temperature_cached = self._unpack_temperature(buf)
			return self._temperature_cached

		except Exception, err:
			print err

		return float("nan")

	def _unpack_humidity(self, buf):
		if len(buf) != 2:
			return float("nan")
		raw = struct.unpack('<H', bytes(buf))[0]
		return float(raw) / 100.0

	@property
	def humidity(self):
		if self._humidity_cached is not None:
			return self._humidity_cached

		try:
			buf = self._humidity.read()
			self._humidity_cached = self._unpack_humidity(buf)
			return self._humidity_cached

		except Exception, err:
			print err

		return float("nan")

	def _unpack_unk1(self, buf):
		if len(buf) != 2:
			return -1
		return struct.unpack('<H', bytes(buf))[0]

	@property
	def unk1(self):
		if self._unk1_cached is not None:
			return self._unk1_cached

		try:
			buf = self._unk1.read()
			self._unk1_cached = self._unpack_unk1(buf)
			return self._unk1_cached

		except Exception, err:
			print err

		return -1

	def _unpack_unk2(self, buf):
		if len(buf) != 1:
			return -1
		return buf[0]

	@property
	def unk2(self):
		if self._unk2_cached is not None:
			return self._unk2_cached

		try:
			buf = self._unk2.read()
			self._unk2_cached = self._unpack_unk2(buf)
			return self._unk2_cached

		except Exception, err:
			print err

		return -1

	@property
	def ready(self):
		return (self.name is not None and self._pressure is not None
			and self._temperature is not None and self._humidity is not None
			and self._unk1 is not None and self._unk2 is not None
			and self.address is not None and self.connectTime is not None)

	def subscribeAll(self):
		assert self.ready
		print "Subscribing to notifications: %s" % self.address

		def _pressure_handler(buf):
			self._pressure_cached = self._unpack_pressure(buf)
		self._pressure.subscribe(_pressure_handler)

		def _temperature_handler(buf):
			self._temperature_cached = self._unpack_temperature(buf)
		self._temperature.subscribe(_temperature_handler)

		def _humidity_handler(buf):
			self._humidity_cached = self._unpack_humidity(buf)
		self._humidity.subscribe(_humidity_handler)

		def _unk1_handler(buf):
			self._unk1_cached = self._unpack_unk1(buf)
		self._unk1.subscribe(_unk1_handler)

		def _unk2_handler(buf):
			self._unk2_cached = self._unpack_unk2(buf)
		self._unk2.subscribe(_unk2_handler)

	def __str__(self):
		return "%s (%s)" % (self.name, self.address)

def runHttpServer(port, client, reset, ready, devices):
	"""Start the HTTP server"""

	class WebServerHandler(BaseHTTPRequestHandler):
		"""Handle HTTP requests and serve a simple web UI"""

		def _serve_main(self):
			"""Serve the main page"""
			ready.wait()

			env = Environment(loader=FileSystemLoader("templates"))
			template = env.get_template("sensorapp.html")

			self.send_response(200, 'OK')
			self.send_header('Content-type', 'html')
			self.end_headers()

			self.wfile.write(template.render(devices=devices))
			self.wfile.close()

		def _serve_favicon(self):
			self.send_response(200, 'OK')
			self.send_header('Content-type', 'png')
			self.end_headers()
			f = open("static/icons/sensorapp.png", "rb")
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
			else:
				self.send_error(404)

	server = HTTPServer(("", port), WebServerHandler)
	try:
		server.serve_forever()
	finally:
		server.socket.close()

def runClient(client, reset, ready, devices):
	"""Start a beetle client"""

	gapUuid = uuid.UUID(gatt.GAP_SERVICE_UUID)
	nameUuid = uuid.UUID(gatt.GAP_CHARAC_DEVICE_NAME_UUID)

	envSenseUuid = uuid.UUID(ENV_SENSING_SERVICE_UUID)
	pressureUuid = uuid.UUID(PRESSURE_CHARAC_UUID)
	temperatureUuid = uuid.UUID(TEMPERATURE_CHARAC_UUID)
	humidityUuid = uuid.UUID(HUMIDITY_CHARAC_UUID)
	unk1Uuid = uuid.UUID(UNK1_CHARAC_UUID)
	unk2Uuid = uuid.UUID(UNK2_CHARAC_UUID)

	beetleUuid = uuid.UUID(beetle.BEETLE_SERVICE_UUID)
	bdAddrUuid = uuid.UUID(beetle.BEETLE_CHARAC_BDADDR_UUID)
	connTimeUuid = uuid.UUID(beetle.BEETLE_CHARAC_CONNECTED_TIME_UUID)

	def _daemon():
		while True:
			del devices[:]
			services = client.discoverAll()

			currDevice = None

			# proceed down the services, separating out devices
			# TODO(james): add Beetle service to include service id

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
								currDevice = SensorInstance(name=currDeviceName)
							except ClientError, err:
								print err
							except Exception, err:
								print err

				elif service.uuid == envSenseUuid:
					for charac in service.characteristics:
						print "  ", charac

						if currDevice is not None:
							if charac.uuid == pressureUuid:
								currDevice._pressure = charac
							elif charac.uuid == temperatureUuid:
								currDevice._temperature = charac
							elif charac.uuid == humidityUuid:
								currDevice._humidity = charac
							elif charac.uuid == unk1Uuid:
								currDevice._unk1 = charac
							elif charac.uuid == unk2Uuid:
								currDevice._unk2 = charac

				elif service.uuid == beetleUuid:
					for charac in service.characteristics:
						print "  ", charac

						if currDevice is not None:
							if charac.uuid == bdAddrUuid:
								try:
									bdaddr = charac.read()[::-1]
									currDevice.address = ":".join(
										"%02X" % x for x in bdaddr)
								except ClientError, err:
									print err
								except Exception, err:
									print err
							elif charac.uuid == connTimeUuid:
								try:
									raw = charac.read()
									if len(raw) != 4:
										continue
									epoch = struct.unpack('<I', bytes(raw))[0]
									currDevice.connectTime = \
										datetime.utcfromtimestamp(epoch)

								except ClientError, err:
									print err
								except Exception, err:
									print err

							print currDevice, currDevice.ready

							if currDevice.ready:
								currDevice.subscribeAll()
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
