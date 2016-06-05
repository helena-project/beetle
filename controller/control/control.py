import socket
import sys
import os
import argparse
import threading
import traceback

def get_arguments():
	"""Arguments for script."""

	parser = argparse.ArgumentParser()
	parser.add_argument("--port", "-p", type=int, default=3003, 
		help="port to listen on")

	return parser.parse_args()

def handle_client(conn, addr):

	def client_reader(conn):
		while True:
			buf = s.recv()
			if buf == "":
				raise RuntimeError("socket connection broken")
			print buf,

	outputThread = threading.Thread(target=client_reader, args=(conn,))
	outputThread.setDaemon(True)
	outputThread.start()

	while True:
		line = raw_input("> ")
		if line != "":
			conn.send(line)

def main(args):
	host = ""
	port = args.port

	s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	try:
	    s.bind((HOST, PORT))
	except socket.error as msg:
	    print 'Bind failed. Error Code : ' + str(msg[0]) + ' Message ' + msg[1]
	   	sys.exit(-1)

	s.listen(5)
	print 'Socket now listening'

	while True:
	    conn, addr = s.accept()
	    print 'Connected with ' + addr[0] + ':' + str(addr[1])

	    handle_client(conn, addr)

	s.close()
if __name__ == "__main__":
	args = getArguments()
	main(args)