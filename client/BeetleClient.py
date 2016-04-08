#!/usr/bin/env python
# 
# Basic Beetle TCP client 
# =======================
# Write hex bytes to Beetle, get hex bytes response back.
 
import os
import signal
import socket
import threading
import time
import re
import traceback
import argparse

def getArguments():
	"""
	Arguments for script.
	"""
	parser = argparse.ArgumentParser()
	parser.add_argument("--host", default="localhost", 
		help="hostname of the Beetle server")
	parser.add_argument("--port", "-p", type=int, default=5001, 
		help="port the server is runnng on")
	return parser.parse_args()

args = getArguments()

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((args.host, args.port))

def outputPrinter(s):
	"""
	Print the output in a separate thread.
	"""
	try:
		while True:
			n = s.recv(1)
			if n == "":
				raise RuntimeError("socket connection broken")
			chunks = []
			received = 0
			n = ord(n)
			while received < n:
				chunk = s.recv(n - received)
				if chunk == "":
					raise RuntimeError("socket connection broken")
				received += len(chunk)
				chunks.append(" ".join(x.encode('hex') for x in chunk))
			print " ".join(chunks)
	except Exception, err:
		print "Exception in output thread:", err
		os.kill(os.getpid(), signal.SIGTERM)

# Send initial connection parameters. Just 0 for now.
paramLength = bytearray(4)
s.send(paramLength)

# Start the printer thread.
outputThread = threading.Thread(target=outputPrinter, args=(s,))
outputThread.setDaemon(True)
outputThread.start()

# Regexes to match convenience commands.
writePattern = re.compile(r"^write (?P<handle>\d+) (?P<value>.*)$")
readPattern = re.compile(r"^read (?P<handle>\d+)$")

def inputReader(s):
	"""
	Consume user input in the main thread.
	"""
	while True:
		line = raw_input("> ")
		line = line.strip().lower()
		
		try: 
			writeCommand = writePattern.match(line)
			readCommand = readPattern.match(line)
			if writeCommand is not None:
				command = writeCommand.groupdict()
				message = bytearray()
				handle = int(command["handle"])
				message.append(0x52)
				message.append(handle & 0xFF)
				message.append(handle >> 8)
				value = command["value"]
				value = value.replace(" ", "")
				message += bytearray.fromhex(value)
			elif readCommand is not None:
				command = readCommand.groupdict()
				message = bytearray()
				message.append(0x0A)
				handle = int(command["handle"])
				message.append(handle & 0xFF)
				message.append(handle >> 8)
			else:
				line = line.replace(" ","")
				message = bytearray.fromhex(line)
		except Exception, err:
			print "Invalid input:", err
			continue

		if len(message) == 0:
			continue
		if s.send(chr(len(message))) != 1:
			raise RuntimeError("failed to write length prefix")
		if s.send(message) != len(message):
			raise RuntimeError("failed to write packet")
		time.sleep(1) # Give beetle some time to respond

try: 
	inputReader(s)
except RuntimeError, err:
	print "Exception in input thread:", err
	print "Exiting..."