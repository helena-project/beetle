
import socket
import os
import sys
import threading
import warnings

from main.constants import SESSION_TOKEN_LEN

from network.models import ConnectedGateway, ConnectedDevice

IPC_COMMAND_PATH = "/tmp/rsb0dsLArGPfOCbWRSIo"

class ManagerException(Exception):
	"""Exception thrown by manager functions"""
	pass

class ManagedGateway(object):

	def __init__(self, conn, ip, port, session_token, name):
		"""A gateway under the remote management of this server

		Args:
			conn : a socket
			ip : string
			port : int 
			session_token : token to find connected gateway instance
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
		assert isinstance(cmd, list)

		cmd = " ".join(cmd).strip()
		print "writing:", cmd

		self._command_mutex.acquire()
		try:
			self._conn.send(str(cmd) + '\n')
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

	def __find_managed_gateway(self, gateway_instance):
		for gateway in self._gateways.values():
			print gateway._session_token, gateway_instance.session_token
			if gateway._session_token == gateway_instance.session_token:
				return gateway
		return None

	def __daemon(self):
		s = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
		try:
			os.remove(IPC_COMMAND_PATH)
		except OSError:
			pass
		s.bind(IPC_COMMAND_PATH)
		s.listen(10)
		while self._running:
			conn, _ = s.accept()
			command = conn.recv(1024)
			if command:
				self.__process_request(command)
			conn.close()

	def __process_request(self, command):
		command = command.split(" ")
		if command[0] == "map":
			from_id = int(command[1])
			to_id = int(command[2])
			try:
				from_device = ConnectedDevice.objects.get(id=from_id)
				to_device = ConnectedDevice.objects.get(id=to_id)
				self.__send_mapping_command(from_device, to_device)
			except ConnectedDevice.DoesNotExist:
				pass

	def __send_mapping_command(self, from_device, to_device):
		print "command:", from_device, to_device
		cmd = None

		if to_device.gateway_instance == from_device.gateway_instance:
			cmd = ["map-local", str(from_device.remote_id), 
				str(to_device.remote_id)]
		else:
			cmd = ["map-remote", from_device.gateway_instance.ip_address,
				str(from_device.gateway_instance.port), 
				str(from_device.remote_id), str(to_device.remote_id)]

		gateway = self.__find_managed_gateway(to_device.gateway_instance)
		if not gateway:
			warnings.warn("gateway not found")
			return
		else:
			gateway.command(cmd)

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
	while n_read < SESSION_TOKEN_LEN * 2:
		chunk = conn.recv(SESSION_TOKEN_LEN * 2 - n_read)
		if chunk == "":
			raise ManagerException("failed to read session token")
		chunks.append(chunk)
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
		conn.shutdown(socket.SHUT_RDWR)
		raise ManagerException("gateway session does not exist")

	name = gateway_instance.gateway.name
	return name, ManagedGateway(conn, ip, port, session_token, name)

def run(host, port):
	"""Run the Beetle manager"""

	manager = Manager()

	s = _setup_server(host, port)
	print "Waiting for clients to connect."

	try:
		while True:
			conn, addr = s.accept()
			try:
				name, gateway = _connect_gateway(conn, addr)
				manager.add_gateway(name, gateway)
			except ManagerException, err:
				print "Caught exception:", err

	except KeyboardInterrupt:
		print "Interrupted..."
	finally:
		manager.shutdown()
