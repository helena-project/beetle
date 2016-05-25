import threading
import socket

from server import GattServer
from client import GattClient

class ManagedSocket:
	def __init__(self, stream=True):
		self._server = None
		self._client = None
		self._sock = None
		self._stream = stream
		self._lock = threading.Lock()

		self._readThread = threading.Thread(target=self.__recv, 
			name="read thread", args=(self,))
		self._readThread.setDaemon(True)

	def bind(self, sock):
		self._sock = sock
		self._readThread.start()

	def shutdown(self):
		self._running = False
		self._sock.shutdown(socket.SHUT_RD)
		self._teardown()

	def _setServer(self, server):
		assert type(server) is GattServer
		self._server = server

	def _setClient(self, client):
		assert type(client) is GattClient
		self._client = client

	def _teardown(self, err=None):
		if self._client is not None:
			self._client._handle_disconnect(err)
		if self._server is not None:
			self._server._handle_disconnect(err)

	def _send(self, pdu):
		assert type(pdu) is bytearray
		
		self._lock.acquire()
		try:
			if self._stream and self.sock._send(chr(len(pdu))) != 1:
				raise RuntimeError("failed to write length prefix")
			if self.sock._send(pdu) != len(pdu):
				raise RuntimeError("failed to write packet")
		except Exception, err:
			self._running = False
			self._teardown(err)
		finally:
			self._lock.release()

	def __recv(self):
		try:
			while self._running:
				if self._stream:
					n = self._sock.recv(1)
					if n == "":
						raise RuntimeError("socket connection broken")
					received = 0
					n = ord(n)
					if n == 0:
						break # teardown
					pdu = bytearray()
					while received < n:
						chunk = self._sock.recv(n - received)
						if chunk == "":
							raise RuntimeError("socket connection broken")
						received += len(chunk)
						pdu += bytearray(ord(x) for x in chunk)
				else:
					pdu = self._sock.recv()
					if pdu == "":
						raise RuntimeError("socket connection broken")
					pdu = bytearray(ord(x) for x in pdu)
				
				op = pdu[0]
				if (att.isRequest(op) 
					or op == att.OP_WRITE_CMD 
					or op == att.OP_SIGNED_WRITE_CMD 
					or att.OP_HANDLE_CNF):

					if self._server is not None:
						resp = self._handle_packet(pdu)
						if resp:
							self._send(resp)
					elif att.isRequest(op):
						resp = att_pdu.new_error_resp(op, 0, 
							att.ECODE_REQ_NOT_SUPP)
						self._send(pdu)

				elif (att.isResponse(op) 
					or op == att.OP_HANDLE_IND 
					or op == att.OP_HANDLE_IND):
					if self._client is not None:
						self._client._handle_packet(pdu)

			# normal teardown
			self._running = False
			self._teardown(None)

		except Exception, err:
			self._running = False
			self._teardown(err)
		
		finally:
			self._sock.close()