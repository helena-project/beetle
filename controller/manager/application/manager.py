
import socket
import sys
import threading
import warnings

from main.constants import SESSION_TOKEN_LEN

from network.models import ConnectedGateway

class ManagerException(Exception):
	"""Exception thrown by manager functions"""
	pass

class ManagedGateway(object):

	def __init__(self, conn, session_token, ip, port, name):
		"""A gateway under the remote management of this server

		Args:
			conn : a socket
			session_token : token to find connected gateway instance
			ip : string
			port : int 
			name : name of the gateway
		"""
		self._conn = conn
		self._ip = ip
		self._port = port
		self._running = True

		self._name = name
		self._session_token = session_token

		def client_reader(conn):
			while self._running:
				buf = conn.recv(2048)
				if buf == "":
					break
				sys.stdout.write(buf)
				sys.stdout.flush()

		read_thread = threading.Thread(target=client_reader, args=(conn,))
		read_thread.setDaemon(True)
		read_thread.start()

		self._command_mutex = threading.Lock()

	def shutdown(self):
		"""Close the managed gateway"""

		self._running = False
		self._conn.shutdown(socket.SHUT_RDWR)
		self._conn.close()

		try:
			gateway_instance = ConnectedGateway.objects.get(
				session_token=self._session_token)
			gateway_instance.is_connected = False
			gateway_instance.save()
		except ConnectedGateway.DoesNotExist:
			warnings.warn("gateway session does not exist")

	def command(self, cmd):
		"""Send a command to the gateway"""

		if not isinstance(cmd, str):
			cmd = " ".join(cmd).strip()
		
		if not cmd:
			warnings.warn("empty command")
			return

		self._command_mutex.acquire()
		try:
			self._conn.send(cmd)
			self._conn.send('\n')
		finally:
			self._command_mutex.release()

class Manager(object):

	def __init__(self):
		"""Manage a set of gateways"""

		self._gateways = {}
		self._running = True

		action_thread = threading.Thread(target=self.__daemon)
		action_thread.setDaemon(True)
		action_thread.start()

	def shutdown(self):
		self._running = False
		for gateway in self._gateways.values():
			gateway.shutdown()

	def add_gateway(self, name, gateway):
		self._gateways[name] = gateway

	def __daemon(self):
		while self._running:
			pass

def _setup_server(host, port):
	"""Creates a socket, binds and listens"""
	s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
	try:
		s.bind((host, port))
	except socket.error as msg:
		print 'Bind failed. Error Code : %d Message %s' % (msg[0], msg[1])
		sys.exit()
	s.listen(5)
	return s

def _read_session_token(conn):
	"""The first SESSION_TOKEN_LEN are issued by Beetle controller"""
	n_read = 0
	chunks = []
	while n_read < SESSION_TOKEN_LEN:
		chunk = conn.recv(SESSION_TOKEN_LEN - n_read)
		if chunk == "":
			raise ManagerException("failed to read session token")
		chunks.append(chunks)
		n_read += len(chunk)
	return "".join(chunks)

def _connect_gateway(conn, addr):
	"""Setup a managed gateway"""
	ip = addr[0]
	port = addr[1]
	print 'Connected with ' + ip + ':' + str(port)

	session_token = _read_session_token(conn)

	try:
		gateway_instance = ConnectedGateway.objects.get(
			session_token=session_token)
		gateway_instance.is_connected = True
		gateway_instance.save()
	except ConnectedGateway.DoesNotExist:
		raise ManagerException("gateway session does not exist")

	return ManagedGateway(conn, ip, port, session_token, 
		gateway_instance.gateway.name)

def run(host, port):
	"""Run the Beetle manager"""

	manager = Manager()

	s = _setup_server(host, port)
	print "Waiting for clients to connect."

	try:
		while True:
			conn, addr = s.accept()
			try:
				gateway = _connect_gateway(conn, addr)
				manager.add_gateway(addr[0], gateway)
			except ManagerException, err:
				print "Caught exception:", err

	except KeyboardInterrupt:
		print "Interrupted..."
	finally:
		manager.shutdown()



