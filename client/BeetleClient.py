#!/usr/bin/env python
# 
# Basic Beetle TCP client 
# =======================
# Write hex bytes to Beetle, get hex bytes response back.
 
import sys
import socket
import thread
import time
import traceback

if len(sys.argv) < 3:
	print "usage: ./BeetleClient.py host port"
	sys.exit(1)

host = sys.argv[1]
port = int(sys.argv[2])

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((host, port))

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
		print "Exception in output thread: ", err
		traceback.print_exc()
		sys.exit(2)

"""
Send initial connection parameters.
"""
paramLength = bytearray(4)
s.send(paramLength)

"""
Start the reader thread.
"""
thread.start_new_thread(outputPrinter, (s,))

"""
Consume user input in the main thread.
"""
while True:
	line = raw_input("> ")
	line = line.strip().replace(" ","")
	message = bytearray.fromhex(line)
	if len(message) == 0:
		continue
	if s.send(chr(len(message))) != 1:
		raise RuntimeError("failed to write length prefix")
	if s.send(message) != len(message):
		raise RuntimeError("failed to write packet")
	time.sleep(3) # Give beetle some time to respond
