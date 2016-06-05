import socket
import sys
import os
import argparse
import threading
import traceback
import time

def get_arguments():
	"""Arguments for script."""

	parser = argparse.ArgumentParser()
	parser.add_argument("--port", "-p", type=int, default=3003, 
		help="port to listen on")

	return parser.parse_args()

def handle_client(conn, addr):

	def client_reader(conn):
		while True:
			buf = conn.recv(2048)
			if buf == "":
				raise RuntimeError("socket connection broken")
			sys.stdout.write(buf)
			sys.stdout.flush()

	outputThread = threading.Thread(target=client_reader, args=(conn,))
	outputThread.setDaemon(True)
	outputThread.start()

	while True:
		line = raw_input()
		conn.send(line + "\n")
		if line == "q" or line == "quit":
			break

def main(args):
	host = ""
	port = args.port

	s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

	try:
		s.bind((host, port))
	except socket.error as msg:
		print 'Bind failed. Error Code : ' + str(msg[0]) + ' Message ' + msg[1]
		sys.exit(-1)

	s.listen(1)
	print 'Waiting for a client to connect.'

	conn, addr = s.accept()

	s.shutdown(socket.SHUT_RDWR)
	s.close()

	print 'Connected with ' + addr[0] + ':' + str(addr[1])
	handle_client(conn, addr)

if __name__ == "__main__":
	args = get_arguments()
	main(args)