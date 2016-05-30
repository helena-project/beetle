import threading
import socket
import traceback

from server import GattServer
from client import GattClient

import lib.att as att

class ManagedSocket:
	def __init__(self, recv_mtu=att.DEFAULT_LE_MTU, 
		send_mtu=att.DEFAULT_LE_MTU, daemon=False):
		"""Create a managed socket wrapping around a regular socket.
		
		Args:
			recv_mtu : the MTU supported by the other side of the connection 
			send_mtu : the MTU supported by this side of the connection
			daemon : if true, then then this class will use a daemon thread
		"""

		assert type(recv_mtu) is int
		assert recv_mtu > 0
		assert type(send_mtu) is int
		assert send_mtu > 0

		self._server = None
		self._client = None

		self._stream = None
		self._sock = None
		self._lock = threading.Lock()
		
		self._recv_mtu = recv_mtu
		self._send_mtu = send_mtu

		self._running = True
		self._readThread = threading.Thread(target=self.__recv)
		self._readThread.setDaemon(daemon)

	def getSendMtu(self):
		"""Return the MTU supported by the other end"""
		return self._send_mtu

	def getRecvMtu(self):
		"""Return the MTU supported by this end"""
		return self._recv_mtu

	def bind(self, sock, stream):
		"""Transfer ownership of socket and starts handling packets.

		Args:
			sock : socket from python socket library
			stream : whether the socket preserves packet boundaries
		"""
		self._stream = stream
		self._sock = sock
		self._readThread.start()

	def shutdown(self):
		"""Kill the managed socket"""
		self._running = False
		self._sock.shutdown(socket.SHUT_RDWR)
		self._teardown()

	def _setServer(self, server):
		assert server.__class__ is GattServer
		self._server = server

	def _setClient(self, client):
		assert client.__class__ is GattClient
		self._client = client

	def _teardown(self, err=None):
		if self._client is not None:
			self._client._handle_disconnect(err)
		if self._server is not None:
			self._server._handle_disconnect(err)

	def _send(self, pdu):
		"""Send an ATT packet"""
		
		assert type(pdu) is bytearray
		assert len(pdu) > 0
		assert len(pdu) <= self._send_mtu

		self._lock.acquire()
		try:
			if self._stream and self._sock.send(chr(len(pdu))) != 1:
				raise RuntimeError("failed to write length prefix")
			if self._sock.send(pdu) != len(pdu):
				raise RuntimeError("failed to write packet")
		except Exception, err:
			self._running = False
			self._teardown(err)
		finally:
			self._lock.release()

	def __recv_single(self):
		"""Read and handle a single packet"""

		if self._stream:
			# Stream mode
			n = self._sock.recv(1)
			if n == "":
				raise RuntimeError("socket connection broken")
			received = 0
			n = ord(n)
			if n == 0:
				return
			pdu = bytearray()
			while received < n:
				chunk = self._sock.recv(n - received)
				if chunk == "":
					raise RuntimeError("socket connection broken")
				received += len(chunk)
				pdu += bytearray(ord(x) for x in chunk)
		else:
			# Datagram mode
			pdu = self._sock.recv()
			if pdu == "":
				raise RuntimeError("socket connection broken")
			pdu = bytearray(ord(x) for x in pdu)
		
		op = pdu[0]

		if op == att.OP_MTU_REQ:
			resp = bytearray(3)
			resp[0] = att.OP_MTU_RESP
			resp[1] = self.mtu & 0xFF
			resp[2] = (self.mtu >> 8) & 0xFF
			self._send(resp)

		elif (att.isRequest(op) or op == att.OP_WRITE_CMD 
			or op == att.OP_SIGNED_WRITE_CMD or op == att.OP_HANDLE_CNF):

			if self._server is not None:
				resp = self._server._handle_packet(pdu)
				if resp:
					self._send(resp)
			elif att.isRequest(op):
				raise RuntimeWarning("server not supported")
				resp = att_pdu.new_error_resp(op, 0, att.ECODE_REQ_NOT_SUPP)
				self._send(resp)

		elif (att.isResponse(op) 
			or (op == att.OP_ERROR and len(pdu) >= 2 and att.isRequest(pdu[1]))
			or op == att.OP_HANDLE_NOTIFY or op == att.OP_HANDLE_IND):

			if self._client is not None:
				self._client._handle_packet(pdu)

		else:
			raise RuntimeWarning("unknown opcode: " + str(op))

	def __recv(self):
		"""Receive packets"""
		try:
			while self._running:
				self.__recv_single()
				
			# normal teardown
			self._running = False
			self._teardown(None)

		except Exception, err:
			traceback.print_exc()
			self._running = False
			self._teardown(err)
		
		finally:
			self._sock.close()

