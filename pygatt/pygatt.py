import threading
import socket

import ble.att as att
import ble.gatt as gatt
from ble.uuid import UUID
import ble.att_pdu as att_pdu

class _Handle:
	def __init__(self, server, uuid):
		assert type(server) is GattServer
		assert type(uuid) is UUID

		server._handle.append(self)

		self._no = server._alloc_handle()
		self._uuid = uuid
		self._readCallback = None
		self._writeCallback = None

	def read(self):
		try:
			return self._readCallback()
		except:
			return None

	def write(self):
		try:
			self._writeCallback()
		except:
			pass

	def _setReadCallback(self, cb):
		self._readCallback = cb

	def _setReadValue(self, value):
		self._readCallback = lambda: value

	def _setWriteCallback(self, cb):
		self._writeCallback = cb

	def _getHandleNoAsBytearray(self):
		arr = bytearray(2)
		arr[0] = self._no & 0xFF
		arr[1] = (self._no >> 8) & 0xFF
		return arr

class Service:
	def __init__(self, server, uuid):
		assert type(server) is GattServer
		assert type(uuid) is UUID

		self.uuid = uuid
		self.characteristics = []

		self._handle = _Handle(server, UUID(gatt.PRIM_SVC_UUID))
		self._handle._setReadValue(uuid.raw())

	def _add_characteristic(self, char):
		self.characteristics.append(char)

	def _get_start_group(self):
		return self._handle._no

	def _get_end_group(self):
		return self.characteristics[-1]._get_end_group() if \
			self.characteristics else self._handle._no

class Characteristic:
	def __init__(self, server, uuid, allowNotify=False, allowIndicate=False):
		assert type(server) is GattServer
		assert type(uuid) is UUID

		self.server = server
		self.uuid = uuid
		self.descriptors = []
		self.properties = 0
		if allowIndicate:
			self.properties |= gatt.CHARAC_PROP_IND
		if allowNotify:
			self.properties |= gatt.CHARAC_PROP_NOTIFY	

		self.service = server.services[-1]
		self.service._add_characteristic(self)

		if allowNotify or allowIndicate:
			self._cccd = Descriptor(server, 
				UUID(gatt.CLIENT_CHARAC_CFG_UUID))
			def cccd_cb(value):
				assert type(value) is bytearray
				if len(value) != 2:
					raise att.ECODE_INVALID_PDU
				else:
					self._cccd.write(value)

			self._cccd._handle.setWriteCallback(cccd_cb)
			self._cccd.write(bytearray([0, 0]))
		else:
			self._cccd = None

		self._handle = _Handle(server, 
			UUID(gatt.CHARAC_UUID))
		self._valHandle = _Handle(server, uuid)

		def read_cb():
			handleValue = bytearray([self.properties])
			handleValue += self._valHandle._getHandleNoAsBytearray()
			handleValue += uuid.raw()
			return handleValue	
		self._handle._setReadCallback(read_cb)
	
	def _add_descriptor(self, descriptor):
		self.descriptors.append(descriptor)

	def _get_start_group(self):
		return self._handle._no

	def _get_end_group(self):
		return self.descriptors[-1]._handle._no if self.descriptors else \
		self._valHandle._no

	def setReadCallback(self, cb):
		self.properties |= gatt.CHARAC_PROP_READ
		self._valHandle._setReadCallback(cb)

	def setWriteCallback(self, cb):
		self.properties |= (gatt.CHARAC_PROP_WRITE | gatt.CHARAC_PROP_WRITE_NR)
		self._valHandle._setWriteCallback(cb)

	def sendNotify(self, value):
		assert type(value) is bytearray
		assert self._cccd != None
		
		cccdValue = self._cccd.read()
		if cccdValue is not None and cccdValue[0] == 1:
			pdu = bytearray([att.OP_HANDLE_NOTIFY])
			pdu += self._valHandle._getHandleNoAsBytearray()
			pdu += value
			self.server._sock._send(pdu)

	def sendIndicate(self, value, cb):
		assert type(value) is bytearray
		assert self._cccd != None

		cccdValue = self._cccd.read()
		if cccdValue is not None and cccdValue[1] == 1:
			self.server._indicateQueue.append(cb)
			pdu = bytearray([att.OP_HANDLE_NOTIFY])
			pdu += self._valHandle._getHandleNoAsBytearray()
			pdu += value
			self.server._sock._send(pdu)
			return True
		else:
			return False

class Descriptor:
	def __init__(self, server, uuid):
		assert type(uuid) is UUID
		assert type(server) is GattServer

		self.uuid = uuid
		self.char = server.services[-1].chars[-1]
		self.char._add_descriptor(self)

		self._handle = _Handle(server, uuid)
		self._value = None

	def write(self, value):
		assert type(value) is bytearray
		self._value = value
		self._handle._setReadValue(self._value)

	def read(self):
		return self._value

class GattServer:
	def __init__(self, socket, mtu=23):
		assert type(socket) is ManagedSocket
		
		self._socket = socket
		self._socket._setServer(self)

		self._handles = [None,]
		self._currHandleOfs = 0
		self._mtu = mtu

		self.services = []

		self._indicateQueue = []

	def __is_valid_handle(self, handleNo):
		return handleNo != 0 and handleNo <len(self._handles)

	def _alloc_handle(self):
		currHandleOfs += 1
		return currHandleOfs

	UNSUPPORTED_REQ = set([
		att.OP_READ_BLOB_REQ, att.OP_READ_MULTI_REQ, 
		att.OP_PREP_WRITE_REQ, att.OP_EXEC_WRITE_REQ, 
		att.OP_SIGNED_WRITE_CMD])

	def _handle_packet(pdu):
		op = pdu[0]
		if op == att.OP_MTU_REQ:
			resp = bytearray(3)
			resp[0] = att.OP_MTU_RESP
			resp[1] = self._mtu & 0xFF
			resp[2] = (self._mtu >> 8) & 0xFF
			return resp

		elif op == att.OP_FIND_INFO_REQ:
			if len(pdu) != 5:
				return  att_pdu.new_error_resp(op, 0, att.ECODE_INVALID_PDU)

			_, startHandle, endHandle = att_pdu.parse_find_info_req(pdu)

			if startHandle == 0 or startHandle > endHandle:
				return att_pdu.new_error_resp(op, startHandle, 
					att.ECODE_INVALID_HANDLE)

			raise NotImplementedError

		elif op == att.OP_FIND_BY_TYPE_REQ:
			if len(pdu) < 7:
				return  att_pdu.new_error_resp(op, 0, att.ECODE_INVALID_PDU)

			_, startHandle, endHandle, uuid, value = \
				att_pdu.parse_find_by_type_req(pdu)

			if startHandle == 0 or startHandle > endHandle:
				return att_pdu.new_error_resp(op, startHandle, 
					att.ECODE_INVALID_HANDLE)

			raise NotImplementedError

		elif op == att.OP_READ_BY_TYPE_REQ:
			if len(pdu) != 7 and len(pdu) != 21:
				return att_pdu.new_error_resp(op, 0, att.ECODE_INVALID_PDU)

			_, startHandle, endHandle, uuid = \
				att_pdu.parse_read_by_type_req(pdu)

			raise NotImplementedError

		elif op == att.OP_READ_BY_GROUP_REQ:
			if len(pdu) != 7 and len(pdu) != 21:
				return att_pdu.new_error_resp(op, 0, att.ECODE_INVALID_PDU)

			_, startHandle, endHandle, uuid = \
				att_pdu.parse_read_by_group_req(pdu)

			raise NotImplementedError

		elif op == att.OP_READ_REQ:
			if len(pdu) < 3:
				return  att_pdu.new_error_resp(op, 0, att.ECODE_INVALID_PDU)

			_, handleNo = att_pdu.parse_read_req(pdu)

			if not self.__is_valid_handle(handleNo):
				return att_pdu.new_error_resp(op, handleNo, 
					att.ECODE_INVALID_HANDLE)
			
			if self.server._handles[handleNo]._readCallback:
				try:
					self.server._handles[handleNo]._readCallback(value)
					resp = bytearray([att.OP_READ_RESP])
					resp += self.server._handles[handleNo]._readCallback()
					return resp
				except int, ecode:
					return att_pdu.new_error_resp(op, handleNo, ecode)
				except Exception, err:
					return att_pdu.new_error_resp(op, handleNo, 
						att.ECODE_UNLIKELY)
			else:
				return att_pdu.new_error_resp(op, 0, 
					att.ECODE_READ_NOT_PERM)

		elif op == att.OP_WRITE_REQ or op == att.OP_WRITE_CMD:
			if len(pdu) < 3:
				return att_pdu.new_error_resp(op, handleNo, 
					att.ECODE_INVALID_PDU)

			_, handleNo, value = att_pdu.parse_write(pdu)

			if not self.__is_valid_handle(handleNo):
				if op == att.OP_WRITE_REQ:
					return att_pdu.new_error_resp(op, handleNo, 
						att.ECODE_INVALID_HANDLE)
				else:
					return None

			if self.server._handles[handleNo]._writeCallback:
				try:
					self.server._handles[handleNo]._writeCallback(value)
					return bytearray([att.OP_WRITE_RESP])
				except int, ecode:
					return att_pdu.new_error_resp(op, handleNo, ecode)
				except Exception, err:
					return att_pdu.new_error_resp(op, handleNo, 
						att.ECODE_UNLIKELY)
			else:
				return att_pdu.new_error_resp(op, 0, 
					att.ECODE_WRITE_NOT_PERM)

		elif op == att.OP_HANDLE_CNF:
			try:
				self._indicateQueue.pop(0)()
			except IndexError:
				pass
			except Exception:
				pass
			return None

		else:
			return att_pdu.new_error_resp(op, 0, att.ECODE_REQ_NOT_SUPP)

	def addService(self, uuid):
		service = Service(self, uuid)
		self.services.append(service)
		return service

	def addCharacteristic(self, uuid, properties):
		return Characteristic(server, uuid, properties)
	
	def addDescriptor(self):
		return Descriptor(server, uuid)

class GattClient:
	def __init__(self, socket):
		assert type(socket) is ManagedSocket
		self._socket = socket
		self._socket._setClient(self)

	def getHandler(self):
		def _handle_packet(packet):
			op = packet[0]
		
		return _handle_packet

class ManagedSocket:
	def __init__(self):
		self._server = None
		self._client = None
		self._sock = None
		self._lock = threading.Lock()

		self._readThread = threading.Thread(target=self.__recv, 
			name="read thread", args=(self,))

	def bind(self, sock):
		self._sock = sock
		self._readThread.start()

	def shutdown(self):
		self._running = False
		self._sock.shutdown(socket.SHUT_RD)
		self._readThread.join()

	def _setServer(self, server):
		assert type(server) is GattServer
		self._server = server

	def _setClient(self, client):
		assert type(client) is GattClient
		self._client = client


	def _send(self, pdu):
		assert type(pdu) is bytearray
		
		self._lock.acquire()
		try:
			if self.sock._send(chr(len(pdu))) != 1:
				raise RuntimeError("failed to write length prefix")
			if self.sock._send(pdu) != len(pdu):
				raise RuntimeError("failed to write packet")
		finally:
			self._lock.release()

	def __recv(self):
		while self._running:
			n = self._sock.recv(1)
			if n == "":
				raise RuntimeError("socket connection broken")
			received = 0
			n = ord(n)
			if n == 0:
				raise RuntimeError("socket connection closed")
			pdu = bytearray()
			while received < n:
				chunk = self._sock.recv(n - received)
				if chunk == "":
					raise RuntimeError("socket connection broken")
				received += len(chunk)
				pdu += bytearray(ord(x) for x in chunk)
			
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
					resp = att_pdu.new_error_resp(op, 0, att.ECODE_REQ_NOT_SUPP)
					self._send(pdu)

			elif (att.isResponse(op) 
				or op == att.OP_HANDLE_IND 
				or op == att.OP_HANDLE_IND):
				if self._client is not None:
					self._client._handle_packet(pdu)

			else:
				raise NotImplementedError