#!/usr/bin/env python
# 
# Basic Beetle TCP client 
# =======================
# Write hex bytes to Beetle, get hex bytes response back.
 
import argparse
import os
import re
import sys
import signal
import socket
import ssl
import struct
import threading
import time
import traceback
import random
import numpy as np

import lib.att as att

def getTimeMillis():
	return int(time.time() * 1000)

def getArguments():
	"""
	Arguments for script.
	"""
	parser = argparse.ArgumentParser()
	parser.add_argument("--host", default="localhost", 
		help="hostname of the Beetle server")
	parser.add_argument("--port", "-p", type=int, default=3002, 
		help="port the server is runnng on")
	parser.add_argument("--measure", "-m", action='store_true', 
		help="print performance measurements")
	parser.add_argument("--debug", "-d", action='store_true', 
		help="print debugging")
	return parser.parse_args()

args = getArguments()

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s = ssl.wrap_socket(s, cert_reqs=ssl.CERT_NONE)	# TODO fix this
s.connect((args.host, args.port))

lastRequestTime = 0.0

# Allow only one transaction at a time
transactSema = threading.BoundedSemaphore(1)

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
				if received == 0:
					opcode = ord(chunk[0])
				received += len(chunk)
				chunks.append(" ".join(x.encode('hex') for x in chunk))
			print " ".join(chunks)

			if att.isResponse(opcode) or opcode == att.OP_ERROR:
				if args.measure:
					now = getTimeMillis()
					print "request at: %d" % (lastRequestTime,)
					print "response at: %d" % (now,)
					print "elapsed: %d" % (now - lastRequestTime,)
				transactSema.release()

	except Exception, err:
		print "Exception in output thread:", err
		os.kill(os.getpid(), signal.SIGTERM)

def readClientParams():
	"""
	Ask the user for params until done.
	"""
	print "Enter connection parameters (\"\" when done): 'param value'"
	paramPattern = re.compile(r"^[^ ]+ .*$")
	params = []
	while True:
		line = raw_input("# ")
		line = line.strip()
		if line == "":
			break
		elif paramPattern.match(line) is None:
			print "not a valid parameter string: 'param value'"
			continue
		else:
			params.append(line)
	return "\n".join(params)

# Send initial connection parameters. Just 0 for now.
clientParams = readClientParams()

clientParamsLength = struct.pack("!i", len(clientParams))
s.send(clientParamsLength.encode('utf-8'))
s.send(clientParams.encode('utf-8'))

# Read parameters in plaintext from server
serverParamsLength = struct.unpack("!i", s.recv(4))[0]
print "Server response:"
for serverParam in s.recv(serverParamsLength).split("\n"):
	print "$ %s" % serverParam.rstrip()

# Start the printer thread.
outputThread = threading.Thread(target=outputPrinter, args=(s,))
outputThread.setDaemon(True)
outputThread.start()

# Regexes to match convenience commands.
writePattern = re.compile(r"^write (?P<handle>[\d+]+) (?P<value>.*)$")
readPattern = re.compile(r"^read (?P<handle>[\d+]+)$")
repeatPattern = re.compile(r"^repeat (?P<ntimes>\d+) (?P<minPause>\d+)-(?P<maxPause>\d+) (?P<value>.*)$")

def printHelp():
	"""
	Print a list of commands.
	"""
	print "  help,h"
	print "  r\t\t\t\tRepeat last line."
	print "  read h|h+ofs\t\t\tRead handle h."
	print "  write h|h+ofs hexVal\t\tWrite (with resp) handle h."
	print "  de ad be ef\t\t\tSend 'deadbeef'. (spaces optional)"
	print "  repeat n min-maxPause pdu\tRepeat the packet n times."

def sendMessage(s, message):
	"""
	Helper to send a ATT packet.
	"""
	opcode = message[0]
	if att.isRequest(opcode):
		transactSema.acquire()
		global lastRequestTime
		lastRequestTime = getTimeMillis()

	if s.send(chr(len(message))) != 1:
		raise RuntimeError("failed to write length prefix")
	if s.send(message) != len(message):
		raise RuntimeError("failed to write packet")

def inputReader(s):
	"""
	Consume user input in the main thread.
	"""
	print "Connection ready (type 'help' for usage):"

	previousLine = ""
	while True:
		line = raw_input("> ")
		line = line.strip().lower()

		if line == "r":
			line = previousLine
		else:
			previousLine = line
		
		if line == "h" or line == "help":
			printHelp()
			continue

		ntimes = 1
		pause = (0, 0)
		try: 
			writeRequest = writePattern.match(line)
			readRequest = readPattern.match(line)
			repeatRequest = repeatPattern.match(line)
			if writeRequest is not None:
				command = writeRequest.groupdict()
				message = bytearray()

				handle = int(eval(command["handle"]))
				message.append(att.OP_WRITE_REQ)
				message.append(handle & 0xFF)
				message.append(handle >> 8)
				value = command["value"]
				value = value.replace(" ", "")
				message += bytearray.fromhex(value)
			elif readRequest is not None:
				command = readRequest.groupdict()
				message = bytearray()
				message.append(att.OP_READ_REQ)
				handle = int(eval(command["handle"]))
				message.append(handle & 0xFF)
				message.append(handle >> 8)
			elif repeatRequest is not None:
				command = repeatRequest.groupdict()
				ntimes = int(command["ntimes"])
				pause = (int(command["minPause"]), int(command["maxPause"]))
				value = command["value"]
				value = value.replace(" ", "")
				message = bytearray.fromhex(value)
			else:
				line = line.replace(" ","")
				message = bytearray.fromhex(line)
		except Exception, err:
			print "Invalid input:", err
			continue

		if len(message) == 0:
			continue

		if ntimes > 1:
			isRequest = att.isRequest(message[0])
			latencies = []

			startTime = getTimeMillis()
			for i in xrange(ntimes):
				# wait before sending next message
				if i > 0 and pause[0] > 0:
					time.sleep(random.randint(*pause))

				sendMessage(s, message)
				if isRequest == True:
					transactSema.acquire()
					latencies.append(getTimeMillis() - lastRequestTime)
					transactSema.release()

			if isRequest == True: # need to wait for last request to finish
				transactSema.acquire()
				endTime = getTimeMillis()
				transactSema.release()
			else:
				endTime = getTimeMillis()

			print "=== repeat ==="
			print "start: %d" % (startTime,)
			print "end: %d" % (endTime,)
			print "elapsed: %dms mean: %0.3fms median: %dms stdev: %0.3fms" % (
				endTime - startTime, np.mean(latencies), np.median(latencies), 
				np.std(latencies))

		else:
			sendMessage(s, message)

		time.sleep(1) # Give beetle some time to respond

try: 
	inputReader(s)
except KeyboardInterrupt, err:
	s.shutdown(socket.SHUT_WR)
except RuntimeError, err:
	print "Exception in input thread:", err
finally:
	print "Exiting..."
	s.close()
