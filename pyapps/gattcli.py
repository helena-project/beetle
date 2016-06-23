#!/usr/bin/env python

"""
pygatt gattcli
==============

This module implements a cli GATT client.
"""

import argparse
import ast
import os
import re
import socket
import ssl
import struct
import sys
import traceback
import operator as op

PYGATT_PATH = "../pygatt"
sys.path.append(PYGATT_PATH)

from pygatt import ManagedSocket, GattClient, ClientError

def getArguments():
	"""Arguments for script."""

	parser = argparse.ArgumentParser()
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
	parser.add_argument("--measure", "-m", action='store_true',
		help="print performance measurements")
	parser.add_argument("--debug", "-d", action='store_true',
		help="print debugging")
	parser.add_argument("--nocerts", "-x", action="store_true",
		help="disable verification and use of client certificates")
	return parser.parse_args()

def printBox(s):
	""" Print a header """
	print "%s\n|| %s ||\n%s" % ("=" * (len(s) + 6), s, "=" * (len(s) + 6))

def readClientParams():
	"""Ask the user for params until done."""

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

def printGattHierarchy(services, indent=2):
	"""Prints the full handle space"""

	charIndent = " " * indent
	descIndent = " " * (indent * 2)
	for service in services:
		print "Service: %s" % str(service.uuid)

		for charac in service.characteristics:
			charDesc = []
			description = charac.userDescription
			if description and isinstance(description, str):
				charDesc.append("desc: %s" % (description,))
			charDesc.append("props: %s" % ("".join(charac.permissions),))
			charDesc.append("handle: %d" % (charac.valHandle,))
			print "%sChar: %s (%s)" % (charIndent, str(charac.uuid),
				", ".join(charDesc))

			for descriptor in charac.descriptors:
				print "%sDescriptor: %s (handle: %d)" % (descIndent,
					str(descriptor.uuid), descriptor.handle)

OPERATORS = {
	ast.Add: op.add, ast.Sub: op.sub, ast.Mult: op.mul,
	ast.Div: op.truediv, ast.Pow: op.pow, ast.BitXor: op.xor,
	ast.USub: op.neg
}

def safeEval(node):
    if isinstance(node, ast.Num): # <number>
        return node.n
    elif isinstance(node, ast.BinOp): # <left> <operator> <right>
        return OPERATORS[type(node.op)](safeEval(node.left), safeEval(node.right))
    elif isinstance(node, ast.UnaryOp): # <operator> <operand> e.g., -1
        return OPERATORS[type(node.op)](safeEval(node.operand))
    else:
        raise TypeError(node)

def parseHandleExpr(expr):
	try:
		return safeEval(ast.parse(expr, mode='eval').body)
	except TypeError, err:
		print "invalid handle:", err
	return -1

def findCharacByValHandle(services, valHandle):
	for service in services:
		for charac in service.characteristics:
			# print "searching: %d for %d" % (charac.valHandle, valHandle)
			if charac.valHandle == valHandle:
				return charac
	return None

def doSubscribe(client, services, cmd):
	if len(cmd) < 2:
		print "usage: subscribe handleExpr"
		return

	handleNo = parseHandleExpr(cmd[1])
	if handleNo < 0:
		return

	charac = findCharacByValHandle(services, handleNo)
	if charac is None:
		print "no characteristic with handleNo %d" % handleNo
		return

	def makeCallback(characteristic):
		# This captures characteristic
		def _callback(value):
			print "Data from", characteristic.uuid
			print ">>>", " ".join("%02x" % x for x in value)
		return _callback

	try:
		charac.subscribe(makeCallback(charac))
	except ClientError, err:
		print "ClientError:", err

def doUnsubscribe(client, services, cmd):
	if len(cmd) < 2:
		print "usage: subscribe handleExpr"
		return

	handleNo = parseHandleExpr(cmd[1])
	if handleNo < 0:
		return

	charac = findCharacByValHandle(services, handleNo)
	if charac is None:
		print "no characteristic with handleNo %d" % handleNo
		return

	try:
		charac.unsubscribe()
	except ClientError, err:
		print "ClientError:", err

def doRead(client, services, cmd):
	if len(cmd) < 2:
		print "usage: read handleExpr"
		return

	handleNo = parseHandleExpr(cmd[1])
	if handleNo < 0:
		return

	charac = findCharacByValHandle(services, handleNo)
	if charac is None:
		print "no characteristic with handleNo %d" % handleNo
		return

	try:
		value = charac.read()
		print " ".join("%02x" % x for x in value)

	except ClientError, err:
		print "ClientError:", err

def doWrite(client, services, cmd):
	if len(cmd) < 3:
		print "usage: write handleExpr ..."
		return

	handleNo = parseHandleExpr(cmd[1])
	if handleNo < 0:
		return

	charac = findCharacByValHandle(services, handleNo)
	if charac is None:
		print "no characteristic with handleNo %d" % handleNo
		return

	buf = None
	try:
		msg = "".join(cmd[2:])
		if not msg:
			return
		buf = bytearray.fromhex(msg)
	except Exception, err:
		print "invalid input:", err
		return

	try:
		charac.write(buf)
	except ClientError, err:
		print "ClientError:", err

def printHelp():
	"""Print a list of commands."""

	print "  help,h"
	print "  subscribe,s handleNo"
	print "  unsubscribe,u handleNo"
	print "  read,r handleNo"
	print "  write,w handleNo"
	print "  quit,q"

def runClient(client):
	"""Take commands from user"""

	services = []
	while True:
		line = raw_input("> ")
		cmd = line.strip().lower().split(" ")

		if not cmd or len(cmd[0]) < 1:
			printGattHierarchy(services)
			continue

		op = cmd[0]
		if op[0] == "d":
			services = client.discoverAll()
			printGattHierarchy(services)

		elif op[0] == "s":
			doSubscribe(client, services, cmd)

		elif op[0] == "u":
			doUnsubscribe(client, services, cmd)

		elif op[0] == "r":
			doRead(client, services, cmd)

		elif op[0] == "w":
			doWrite(client, services, cmd)

		elif op[0] == "h":
			printHelp()

		elif op[0] == "q":
			return

		else:
			print "unrecognized command"

def main(args):
	"""Set up a GATT client"""

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

	# Get initial connection parameters.
	appParams = readClientParams()

	# Send connection request parameters to Beetle
	appParamsLength = struct.pack("!i", len(appParams))
	s.sendall(appParamsLength.encode('utf-8'))
	s.sendall(appParams.encode('utf-8'))

	# Read parameters in plaintext from Beetle
	serverParamsLength = struct.unpack("!i", s.recv(4))[0]

	print ""
	printBox("Beetle response")
	for serverParam in s.recv(serverParamsLength).split("\n"):
		print "$ %s" % serverParam.rstrip()

	# Transfer ownership of the socket
	managedSocket.bind(s, True)

	runClient(client)

if __name__ == "__main__":
	try:
		args = getArguments()
		main(args)
	except KeyboardInterrupt, err:
		traceback.print_exc()
		os._exit(0)
