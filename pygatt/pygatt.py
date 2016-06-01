"""
pygatt module
=============

This module implements GATT above the socket layer. For a usage example see 
example.py.
"""

import threading
import socket
import traceback
import warnings

import lib.att as att
import lib.att_pdu as att_pdu
import lib.gatt as gatt
from lib.uuid import UUID

class ServerError(Exception):
	"""Standard error type raised by GattServer.
	
	Note: may safely be raised by callbacks
	"""
	pass

class ClientError(Exception):
	"""Standard error type raised by GattClient.

	Note: may safely be raised by callbacks
	"""
	pass

#------------------------------------------------------------------------------

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

	def _set_server(self, server):
		assert server.__class__ is GattServer
		self._server = server

	def _set_client(self, client):
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
		if len(pdu) > self._send_mtu:
			tmp = " ".join("%02x" % x for x in pdu)
			warnings.warn("pdu exceeds mtu: %s" % tmp)
			pdu = pdu[:self._send_mtu]

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
			# Seqpacket mode
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
				warnings.warn("server not supported")
				resp = att_pdu.new_error_resp(op, 0, att.ECODE_REQ_NOT_SUPP)
				self._send(resp)

		elif (att.isResponse(op) 
			or (op == att.OP_ERROR and len(pdu) >= 2 and att.isRequest(pdu[1]))
			or op == att.OP_HANDLE_NOTIFY or op == att.OP_HANDLE_IND):

			if self._client is not None:
				self._client._handle_packet(pdu)

		else:
			warnings.warn("unknown opcode: " + str(op))

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

#------------------------------------------------------------------------------

class _ServerHandle:
	"""Protected server handle"""
	
	def __init__(self, server, owner, uuid):
		assert isinstance(server, GattServer)
		assert isinstance(uuid, UUID)
		assert owner is not None
		assert owner._get_end_group_handle is not None
		assert owner._get_start_group_handle is not None

		server._add_handle(self)

		self._no = server._alloc_handle()
		self._uuid = uuid
		self._read_callback = None
		self._write_callback = None
		self._owner = owner

	def _read(self):
		try:
			return self._read_callback()
		except ServerError, err:
			return None

	def _write(self, value):
		assert type(value) is bytearray
		try:
			self._write_callback(value)
		except ServerError, err:
			pass

	def __convert_to_bytearray(self, value):
		if type(value) is bytearray:
			return value
		elif type(value) is str:
			return bytearray(ord(x) for x in value)
		else:
			raise ValueError("unsupported value type: " + str(type(value)))

	def _set_read_callback(self, cb):
		def __wrapper():
			return self.__convert_to_bytearray(cb())
		self._read_callback = __wrapper

	def _set_read_value(self, value):
		def __wrapper():
			return self.__convert_to_bytearray(value)
		self._read_callback = __wrapper

	def _set_write_callback(self, cb):
		self._write_callback = cb

	def _get_handle_as_bytearray(self):
		return att_pdu.pack_handle(self._no)

	def __str__(self):
		return "Handle - %d" % self._no

class _ServerService:
	"""Protected server service"""

	def __init__(self, server, uuid):
		assert isinstance(server, GattServer)
		assert isinstance(uuid, UUID)

		# Public members
		self.uuid = uuid
		self.characteristics = []

		# Protected members
		self._handle = _ServerHandle(server, self, UUID(gatt.PRIM_SVC_UUID))

		# reversed by convention
		self._handle._set_read_value(uuid.raw()[::-1])

	def _add_characteristic(self, char):
		self.characteristics.append(char)

	def _get_start_group_handle(self):
		return self._handle

	def _get_end_group_handle(self):
		return self.characteristics[-1]._get_end_group_handle() if \
			self.characteristics else self._handle

	def __len__(self):
		return len(self.characteristics)

	def __str__(self):
		return "Service - %s" % str(self.uuid)

class _ServerCharacteristic:
	def __init__(self, server, uuid, staticValue=None, allowNotify=False, 
		allowIndicate=False):
		assert isinstance(server, GattServer)
		assert isinstance(uuid, UUID)

		# Public members
		self.server = server
		self.uuid = uuid
		self.descriptors = []
		self.service = server.services[-1]
		
		self.service._add_characteristic(self)

		# Protected members
		self._properties = 0
		if staticValue:
			self._properties |= gatt.CHARAC_PROP_READ
		if allowNotify:
			self._properties |= gatt.CHARAC_PROP_NOTIFY	
		if allowIndicate:
			self._properties |= gatt.CHARAC_PROP_IND

		self._has_subscriber = False
		self._subscribe_callback = lambda: None
		self._unsubscribe_callback = lambda: None

		self._handle = _ServerHandle(server, self, UUID(gatt.CHARAC_UUID))
		self._valHandle = _ServerHandle(server, self, self.uuid)

		if allowNotify or allowIndicate:
			self._cccd = _ServerDescriptor(server, 
				UUID(gatt.CLIENT_CHARAC_CFG_UUID))
			def cccd_cb(value):
				assert type(value) is bytearray
				if len(value) != 2:
					raise ServerError("invalid cccd pdu")
				else:
					if value[0] == 1 or value[1] == 1:
						if not self._has_subscriber:
							self._subscribe_callback(value)
							self._has_subscriber = True
					else:
						if self._has_subscriber:
							self._unsubscribe_callback()
							self._has_subscriber = False
					self._cccd.write(value)

			self._cccd._handle._set_write_callback(cccd_cb)
			self._cccd.write(bytearray([0, 0]))
		else:
			self._cccd = None

		if staticValue:
			self._valHandle._set_read_value(staticValue) 

		def read_cb():
			"""Properties may change."""
			handleValue = bytearray([self._properties])
			handleValue += self._valHandle._get_handle_as_bytearray()
			handleValue += self.uuid.raw()[::-1]
			return handleValue	
		self._handle._set_read_callback(read_cb)

	# Public methods

	def setReadCallback(self, cb):
		"""Allow reads and sets a callback to handle client reads.

		Args: 
			cb : a function that returns a bytearray
		"""
		self._properties |= gatt.CHARAC_PROP_READ
		self._valHandle._set_read_callback(cb)

	def setReadValue(self, value):
		"""Allow reads and sets a static value.
		
		Args: 
			value : a bytearray, should be shorter than the the send MTU - 3
		"""
		self._properties |= gatt.CHARAC_PROP_READ
		self._valHandle._set_read_value(value)

	def setWriteCallback(self, cb):
		"""Allow writes and sets a callback to handle client writes

		Args:
			cb : a function that takes a bytearray as argument
		"""
		self._properties |= (gatt.CHARAC_PROP_WRITE | gatt.CHARAC_PROP_WRITE_NR)
		self._valHandle._set_write_callback(cb)

	def setSubscribeCallback(self, cb):
		"""Sets a callback for subscription changes

		Args:
			cb : a function that takes a bytearray of length 2. 
		"""
		if self._cccd is None:
			raise ServerError("subscriptions not allowed")
		self._subscribe_callback = cb

	def setUnsubscribeCallback(self, cb):
		"""Sets a callback for subscription changes

		Args:
			cb : a function that takes no arguments.
		"""
		if self._cccd is None:
			raise ServerError("subscriptions not allowed")
		self._unsubscribe_callback = cb

	def sendNotify(self, value):
		"""Send a notification to the client.

		Args:
			value : a bytearray of length with max length of send MTU - 3
		"""
		assert type(value) is bytearray
		assert self._cccd != None
		
		cccdValue = self._cccd._read()
		if cccdValue is not None and cccdValue[0] == 1:
			pdu = bytearray([att.OP_HANDLE_NOTIFY])
			pdu += self._valHandle._get_handle_as_bytearray()
			pdu += value
			self.server._socket._send(pdu)

	def sendIndicate(self, value, cb):
		"""Send an indication and sets a callback for the confirmation. 

		Args:
			value : a bytearray of length with max length of send MTU - 3
		Returns: 
			Whether the indicate packet was successfully sent.
		"""
		assert type(value) is bytearray
		assert self._cccd != None

		cccdValue = self._cccd._read()
		if cccdValue is not None and cccdValue[1] == 1:
			self.server._indicateQueue.append(cb)
			pdu = bytearray([att.OP_HANDLE_NOTIFY])
			pdu += self._valHandle._get_handle_as_bytearray()
			pdu += value
			self.server._socket._send(pdu)
			return True
		else:
			return False

	# Private and protected methods

	def _add_descriptor(self, descriptor):
		self.descriptors.append(descriptor)

	def _get_start_group_handle(self):
		return self._handle

	def _get_end_group_handle(self):
		endHandle = self.descriptors[-1]._handle if self.descriptors else \
			self._valHandle
		if self._cccd:
			return endHandle if endHandle._no > self._cccd._handle._no else \
				self._cccd._handle
		else:
			return endHandle

	def __len__(self):
		return len(self.descriptors) + 1

	def __str__(self):
		return "Characteristic - %s" % str(self.uuid)

class _ServerDescriptor:
	def __init__(self, server, uuid, value=None):
		assert isinstance(server, GattServer)
		assert isinstance(uuid, UUID)

		# Public members
		self.server = server
		self.uuid = uuid
		self.characteristic = server.services[-1].characteristics[-1]

		self.characteristic._add_descriptor(self)

		# Protected members
		self._handle = _ServerHandle(server, self.characteristic, uuid)
		self._value = value

		if self._value:
			self._handle._set_read_value(value)

	def write(self, value):
		"""Write a value to the descriptor.
		
		Args:
			value : a bytearray with length not exceeding send MTU - 3
		"""
		assert type(value) is bytearray
		self._value = value
		self._handle._set_read_value(self._value)

	def read(self):
		"""Read the descriptor value.

		Returns: 
			A bytearray
		"""
		return self._value

	def __str__(self):
		return "Descriptor - %s" % str(self.uuid)

class GattServer:
	def __init__(self, socket, onDisconnect=None):
		"""Make a GATT server

		Args:
			socket : a ManagedSocket instance
			onDisconnect : callback on disconnect
		"""
		assert isinstance(socket, ManagedSocket)

		# Public members
		self.services = []

		# Protected members
		self._socket = socket
		self._socket._set_server(self)

		self._handles = [None,]
		self._currHandleOfs = 0

		self._indicateQueue = []

		self._onDisconnect = onDisconnect

	# Public methods

	def addGapService(self, deviceName):
		"""Add a Generic Access Profile service.

		Args:
			deviceName : name of the device advertised to the other side
		"""
		assert len(self._handles) == 1
		assert type(deviceName) is str

		self.addService(gatt.GAP_SERVICE_UUID)
		self.addCharacteristic(gatt.GAP_CHARAC_DEVICE_NAME_UUID, deviceName)

	def addService(self, uuid):
		"""Add a service of type uuid.
		
		Returns:
			The newly added service.
		"""
		if not isinstance(uuid, UUID):
			uuid = UUID(uuid)

		service = _ServerService(self, uuid)
		self.services.append(service)
		return service

	def addCharacteristic(self, uuid, value=None, allowNotify=False, 
		allowIndicate=False):
		"""Add a characteristic to the last service.
		
		Notes: permissions are learned by adding callbacks and values to the
		characteristic.

		Args:
			uuid : type of the characteristic
			value : static value for the characteristic
			allowNotify : allow notifications
			allowIndicate :	allow indications
		Returns:
			The newly added characteristic
		"""
		if not isinstance(uuid, UUID):
			uuid = UUID(uuid)

		return _ServerCharacteristic(self, uuid, value, allowNotify, 
			allowIndicate)
	
	def addDescriptor(self, uuid, value=None):
		"""Add a descriptor to the last characteristic

		Args:
			uuid : the type of the descriptor
			value : default value for descriptor
		Returns:
			The newly added characteristic
		"""
		if not isinstance(uuid, UUID):
			uuid = UUID(uuid)

		return _ServerDescriptor(self, uuid, value)

	# Protected and private methods

	def __is_valid_handle(self, handleNo):
		return handleNo != 0 and handleNo < len(self._handles)

	def _alloc_handle(self):
		self._currHandleOfs += 1
		return self._currHandleOfs

	def _add_handle(self, handle):
		self._handles.append(handle)

	def _handle_disconnect(self, err=None):
		if self._onDisconnect is not None:
			 self._onDisconnect(err)

	UNSUPPORTED_REQ = set([
		att.OP_READ_BLOB_REQ, att.OP_READ_MULTI_REQ, 
		att.OP_PREP_WRITE_REQ, att.OP_EXEC_WRITE_REQ, 
		att.OP_SIGNED_WRITE_CMD])

	def __handle_find_info_req(self, op, pdu):
		if len(pdu) != 5:
			return att_pdu.new_error_resp(op, 0, att.ECODE_INVALID_PDU)

		_, startHandle, endHandle = att_pdu.parse_find_info_req(pdu)

		if startHandle == 0 or startHandle > endHandle:
			return att_pdu.new_error_resp(op, startHandle, 
				att.ECODE_INVALID_HANDLE)

		respCount = 0
		resp = bytearray([att.OP_FIND_INFO_RESP])
		respFmt = None
		for handle in self._handles[startHandle:endHandle+1]:
			if respFmt is None:
				respFmt = len(handle._uuid)
				resp.append(1 if respFmt == 2 else 2)
			if len(handle._uuid) != respFmt:
				break
			if len(resp) + 2 + respFmt > self._socket.getSendMtu():
				break
			resp += handle._get_handle_as_bytearray()
			resp += handle._uuid.raw()[::-1]
			respCount += 1

		if respCount == 0:
			return att_pdu.new_error_resp(op, startHandle, \
				att.ECODE_ATTR_NOT_FOUND)
		else:
			return resp

	def __handle_find_by_type_req(self, op, pdu):
		if len(pdu) < 7:
			return att_pdu.new_error_resp(op, 0, att.ECODE_INVALID_PDU)

		_, startHandle, endHandle, uuid, value = \
			att_pdu.parse_find_by_type_req(pdu)

		if startHandle == 0 or startHandle > endHandle:
			return att_pdu.new_error_resp(op, startHandle, 
				att.ECODE_INVALID_HANDLE)

		respCount = 0
		resp = bytearray([att.OP_FIND_BY_TYPE_RESP])
		for handle in self._handles[startHandle:endHandle+1]:
			if handle._uuid != uuid:
				continue
			if len(resp) + 4 > self._socket.getSendMtu():
				break
			resp += handle._get_handle_as_bytearray()
			resp += handle._owner._get_end_group_handle()._get_handle_as_bytearray()
			respCount += 1

		if respCount == 0:
			return att_pdu.new_error_resp(op, startHandle, \
				att.ECODE_ATTR_NOT_FOUND)
		else:
			return resp

	def __handle_read_by_type_req(self, op, pdu):
		if len(pdu) != 7 and len(pdu) != 21:
			return att_pdu.new_error_resp(op, 0, att.ECODE_INVALID_PDU)

		_, startHandle, endHandle, uuid = \
			att_pdu.parse_read_by_type_req(pdu)

		if startHandle == 0 or startHandle > endHandle:
			return att_pdu.new_error_resp(op, startHandle, 
				att.ECODE_INVALID_HANDLE)

		respCount = 0
		resp = bytearray([att.OP_READ_BY_TYPE_RESP, 0]) # 0 is placeholder
		valLen = None
		for handle in self._handles[startHandle:endHandle+1]:
			if handle._uuid != uuid:
				continue

			valRead = handle._read()
			if valRead is None:
				return att_pdu.new_error_resp(op, startHandle, 
					att.ECODE_READ_NOT_PERM)

			assert type(valRead) is bytearray
			if not valLen:
				assert len(valRead) <= 0xFF - 2
				valLen = len(valRead)
				resp[1] = valLen + 2
			if valLen != len(valRead):
				break
			if len(resp) + 2 + valLen > self._socket.getSendMtu():
				break
			resp += handle._get_handle_as_bytearray()
			resp += valRead
			respCount += 1

		if respCount == 0:
			return att_pdu.new_error_resp(op, startHandle, \
				att.ECODE_ATTR_NOT_FOUND)
		else:
			return resp

	def __handle_read_by_group_req(self, op, pdu):
		if len(pdu) != 7 and len(pdu) != 21:
			return att_pdu.new_error_resp(op, 0, att.ECODE_INVALID_PDU)

		_, startHandle, endHandle, uuid = \
			att_pdu.parse_read_by_group_req(pdu)

		if startHandle == 0 or startHandle > endHandle:
			return att_pdu.new_error_resp(op, startHandle, 
				att.ECODE_INVALID_HANDLE)

		respCount = 0
		resp = bytearray([att.OP_READ_BY_GROUP_RESP, 0]) # 0 is placeholder
		valLen = None
		for handle in self._handles[startHandle:endHandle+1]:
			if handle._uuid != uuid:
				continue

			valRead = handle._read()
			if valRead is None:
				return att_pdu.new_error_resp(op, startHandle, 
					att.ECODE_READ_NOT_PERM)

			assert type(valRead) is bytearray
			if not valLen:
				assert len(valRead) <= 0xFF - 4 
				valLen = len(valRead)
				resp[1] = valLen + 4
			if valLen != len(valRead):
				break
			if len(resp) + 4 + valLen > self._socket.getSendMtu():
				break
			resp += handle._get_handle_as_bytearray()
			resp += handle._owner._get_end_group_handle()._get_handle_as_bytearray()
			resp += valRead
			respCount += 1

		if respCount == 0:
			return att_pdu.new_error_resp(op, startHandle,
				att.ECODE_ATTR_NOT_FOUND)
		else:
			return resp

	def __handle_read_req(self, op, pdu):
		if len(pdu) < 3:
			return  att_pdu.new_error_resp(op, 0, att.ECODE_INVALID_PDU)

		_, handleNo = att_pdu.parse_read_req(pdu)

		if not self.__is_valid_handle(handleNo):
			return att_pdu.new_error_resp(op, handleNo, 
				att.ECODE_INVALID_HANDLE)
		
		if self._handles[handleNo]._read_callback:
			try:
				value = self._handles[handleNo]._read_callback()
				resp = bytearray([att.OP_READ_RESP])
				resp += value
				return resp
			except int, ecode:
				return att_pdu.new_error_resp(op, handleNo, ecode)
			except ServerError, err:
				return att_pdu.new_error_resp(op, handleNo, 
					att.ECODE_UNLIKELY)
		else:
			return att_pdu.new_error_resp(op, 0, 
				att.ECODE_READ_NOT_PERM)

	def __handle_write_cmd_req(self, op, pdu):
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

		if self._handles[handleNo]._write_callback:
			try:
				self._handles[handleNo]._write_callback(value)
				return bytearray([att.OP_WRITE_RESP])
			except int, ecode:
				return att_pdu.new_error_resp(op, handleNo, ecode)
			except ServerError, err:
				return att_pdu.new_error_resp(op, handleNo, 
					att.ECODE_UNLIKELY)
		else:
			return att_pdu.new_error_resp(op, 0, 
				att.ECODE_WRITE_NOT_PERM)

	def __handle_confirm(self, op, pdu):
		try:
			self._indicateQueue.pop(0)()
		except IndexError:
			pass
		except ServerError, err:
			pass

	def _handle_packet(self, pdu):
		op = pdu[0]
		if op == att.OP_FIND_INFO_REQ:
			return self.__handle_find_info_req(op, pdu)
		elif op == att.OP_FIND_BY_TYPE_REQ:
			return self.__handle_find_by_type_req(op, pdu)
		elif op == att.OP_READ_BY_TYPE_REQ:
			return self.__handle_read_by_type_req(op, pdu)
		elif op == att.OP_READ_BY_GROUP_REQ:
			return self.__handle_read_by_group_req(op, pdu)
		elif op == att.OP_READ_REQ:
			return self.__handle_read_req(op, pdu)
		elif op == att.OP_WRITE_REQ or op == att.OP_WRITE_CMD:
			return self.__handle_write_cmd_req(op, pdu)
		elif op == att.OP_HANDLE_CNF:
			self.__handle_confirm(op, pdu)
			return None
		else:
			return att_pdu.new_error_resp(op, 0, att.ECODE_REQ_NOT_SUPP)

	def __str__(self, indent=2):
		tmp = []
		for service in self.services:
			tmp.append(str(service))
			for charac in service.characteristics:
				tmp.append(" " * indent + str(charac))
				for descriptor in charac.descriptors:
					tmp.append(" " * indent * 2 + str(descriptor))
		return "\n".join(tmp)

#------------------------------------------------------------------------------

def _handle_to_bytearray(handle):
	"""Packs the 16-bit handle into a little endian bytearray"""
	assert handle <= 0xFFFF
	assert handle >= 0
	return bytearray([handle & 0xFF, (handle >> 8) & 0xFF])

class _ClientService:
	def __init__(self, client, uuid, handleNo, endGroup):
		assert isinstance(uuid, UUID)
		assert isinstance(client, GattClient)
		assert type(handleNo) is int
		assert type(endGroup) is int

		# Public members
		self.client = client
		self.uuid = uuid
		self.characteristics = []

		# Protected members
		self._handleNo = handleNo
		self._endGroup = endGroup 

		self._characteristicHandles = {}

	# Public methods

	def discoverCharacteristics(self, uuid=None):
		"""Find characteristics under the service

		Notes: if uuid is None, all characteristics are discovered. Any 
			previously discovered characteristics are invalidated.
		Args:
			uuid : a specific uuid to discover
		Returns:
			A list of discovered characteristics
		"""
		if uuid and not isinstance(uuid, UUID):
			uuid = UUID(uuid)

		allCharacs, allCharacHandles = self.__discover_all_characteristics()
		if uuid is None:
			self._characteristicHandles = allCharacHandles
			self.characteristics = allCharacs
			return allCharacs
		else:
			characs = filter(lambda x: x.uuid == uuid, allCharacs)
			for charac in characs:
				if charac._handleNo not in self._characteristicHandles:
					self._characteristicHandles[charac._handleNo] = charac
					self.characteristics.append(charac)
			self.characteristics.sort(key=lambda x: x._handleNo)
			return characs

	# Protected and private methods

	def __discover_all_characteristics(self):
		startHandle = self._handleNo + 1
		endHandle = self._endGroup
		if startHandle > endHandle:
			return [], {}
			
		characUuid = UUID(gatt.CHARAC_UUID)

		characs = []
		characHandles = {}
		currHandle = startHandle
		while True:
			req = att_pdu.new_read_by_type_req(currHandle, endHandle, 
				characUuid)

			resp = self.client._new_transaction(req)
			if not resp:
				raise ClientError("transaction failed")

			if resp[0] == att.OP_READ_BY_TYPE_RESP:
				attDataLen = resp[1]
				idx = 2

				maxHandleNo = currHandle # not needed
				while idx < len(resp) and idx + attDataLen <= len(resp): 
					handleNo = att_pdu.unpack_handle(resp, idx)
					properties = resp[idx+2]
					valHandleNo = att_pdu.unpack_handle(resp, idx + 3)
					uuid = UUID(resp[idx+5:idx+attDataLen], reverse=True)

					charac = _ClientCharacteristic(self.client, self, uuid, 
						handleNo, properties, valHandleNo)
					characs.append(charac)
					characHandles[handleNo] = characs

					idx += attDataLen
					maxHandleNo = valHandleNo

				currHandle = maxHandleNo + 1
				if currHandle >= endHandle:
					break

			elif (resp[0] == att.OP_ERROR and len(resp) == att_pdu.ERROR_PDU_LEN 
				and resp[1] == att.OP_READ_BY_TYPE_REQ
				and resp[4] == att.ECODE_ATTR_NOT_FOUND):
				break

			elif (resp[0] == att.OP_ERROR and len(resp) == att_pdu.ERROR_PDU_LEN 
				and resp[1] == att.OP_READ_BY_TYPE_REQ):
				raise ClientError("error - %s" % att.ecodeLookup(resp[4]))

			else:
				raise ClientError("unexpected - %s" % att.opcodeLookup(resp[0]))

		for i, charac in enumerate(characs):
			if i + 1 < len(characs):
				charac._set_end_group(characs[i+1]._handleNo-1)
			else:
				charac._set_end_group(self._endGroup)

		return characs, characHandles

	def __len__(self):
		return len(characteristics)

	def __str__(self):
		return "Service - %s" % str(self.uuid)

class _ClientCharacteristic:
	def __init__(self, client, service, uuid, handleNo, properties, 
		valHandleNo, endGroup=None):
		assert isinstance(uuid, UUID)
		assert isinstance(client, GattClient)
		assert isinstance(service, _ClientService)
		assert type(handleNo) is int
		assert type(valHandleNo) is int
		assert type(properties) is int

		# Public members
		
		self.client = client
		self.uuid = uuid
		self.service = service
	 	self.permissions = set()
		
		if (properties & gatt.CHARAC_PROP_READ) != 0:
			self.permissions.add('r')
		if (properties & gatt.CHARAC_PROP_WRITE) != 0:
			self.permissions.add('w')
		if (properties & gatt.CHARAC_PROP_IND) != 0:
			self.permissions.add('i')
		if (properties & gatt.CHARAC_PROP_NOTIFY) != 0:
			self.permissions.add('n')

		self.client._add_val_handle(self, valHandleNo)

		# Protected members

		self._handleNo = handleNo
		self._valHandleNo = valHandleNo
		self._endGroup = endGroup

		self._subscribeCallback = None

		self._cccd = None
		self.descriptors = []

	# Public methods

	def discoverDescriptors(self):
		"""Return a list of descriptors"""

		assert self._endGroup is not None
		
		startHandle = self._handleNo + 1
		endHandle = self._endGroup
		if startHandle > endHandle:
			return []
		
		descriptors = []

		cccdUuid = UUID(gatt.CLIENT_CHARAC_CFG_UUID)
		cccd = None
		
		currHandle = startHandle
		while True:
			req = att_pdu.new_find_info_req(currHandle, endHandle)

			resp = self.client._new_transaction(req)
			if not resp:
				raise ClientError("transaction failed")

			if resp[0] == att.OP_FIND_INFO_RESP:
				attDataLen = 4 if resp[1] == att.FIND_INFO_RESP_FMT_16BIT \
					else 18
				idx = 2

				handleNo = currHandle # not needed
				while idx < len(resp) and idx + attDataLen <= len(resp): 
					handleNo = att_pdu.unpack_handle(resp, idx)
					uuid = UUID(resp[idx+2:idx+attDataLen], reverse=True)

					if handleNo == self._valHandleNo:
						idx += attDataLen
						continue

					descriptor = _ClientDescriptor(self.client, self, uuid, 
						handleNo)
					if uuid == cccdUuid:
						# hide the cccd from users
						cccd = descriptor
					else:
						descriptors.append(descriptor)

					idx += attDataLen

				currHandle = handleNo + 1
				if currHandle >= endHandle:
					break

			elif (resp[0] == att.OP_ERROR and len(resp) == att_pdu.ERROR_PDU_LEN 
				and resp[1] == att.OP_FIND_INFO_REQ
				and resp[4] == att.ECODE_ATTR_NOT_FOUND):
				break

			elif (resp[0] == att.OP_ERROR and len(resp) == att_pdu.ERROR_PDU_LEN 
				and resp[1] == att.OP_FIND_INFO_REQ):
				raise ClientError("error - %s" % att.ecodeLookup(resp[4]))

			else:
				raise ClientError("unexpected - %s" % att.opcodeLookup(resp[0]))

		self.descriptors = descriptors
		self._cccd = cccd
		return descriptors

	def subscribe(self, cb):
		"""Subscribe to the characteristic, setting a callback.

		Args:
			cb : a function that takes a bytearray
		Raises:
			ClientError on failure
		"""

		if "i" not in self.permissions and "n" not in self.permissions:
			raise ClientError("subscription not permitted")

		if self._cccd is None:
			raise ClientError("need to discover cccd first!")

		self._subscribeCallback = cb

		value = bytearray(2)
		value[0] = 1 if "n" in self.permissions else 0
		value[1] = 1 if "i" in self.permissions else 0

		self._cccd.write(value)

	def unsubscribe(self):
		"""Unsubscribe from the characteristic."""

		self._subscribeCallback = None

		if self._cccd is not None:
			self._cccd.write( ytearray([0,0]))

	def read(self):
		"""Blocking read of the characteristic.

		Returns: 
			A bytearray
		Raises:
			ClientError on failure
		"""
		if "r" not in self.permissions:
			raise ClientError("read not permitted")

		req = bytearray([att.OP_READ_REQ])
		req += _handle_to_bytearray(self._valHandleNo)
		resp = self.client._new_transaction(req)
		if resp is None:
			raise ClientError("no response")
		elif resp[0] == att.OP_READ_RESP:
			return resp[1:]
		elif resp[0] == att.OP_ERROR and len(resp) == att_pdu.ERROR_PDU_LEN:
			raise ClientError("read failed - %s" % att.ecodeLookup(resp[4]))
		else:
			raise ClientError("unexpected - %s" % att.opcodeLookup(resp[0]))

	def write(self, value):
		"""Blocking write of the characteristic.

		Args:
			value : a bytearray of length at most send MTU - 3
		Raises:
			ClientError on failure
		"""
		assert type(value) is bytearray
		if "w" not in self.permissions:
			raise ClientError("write not permitted")

		req = bytearray([att.OP_WRITE_REQ])
		req += _handle_to_bytearray(self._valHandleNo)
		req += value

		resp = self.client._new_transaction(req)
		if resp is None:
			raise ClientError("no response")
		elif resp[0] == att.OP_WRITE_RESP:
			return resp[1:]
		elif resp[0] == att.OP_ERROR and len(resp) == att_pdu.ERROR_PDU_LEN:
			raise ClientError("write failed - %s" % att.ecodeLookup(resp[4]))
		else:
			raise ClientError("unexpected - %s" % att.opcodeLookup(resp[0]))

	# Private and protected methods

	def _set_end_group(self, endGroup):
		self._endGroup = endGroup

	def __len__(self):
		return len(descriptors)

	def __str__(self):
		return "Characteristic - %s" % str(self.uuid)

class _ClientDescriptor:
	def __init__(self, client, characteristic, uuid, handleNo):
		assert isinstance(uuid, UUID)
		assert isinstance(client, GattClient)
		assert isinstance(characteristic, _ClientCharacteristic)
		assert type(handleNo) is int

		# Public members
		self.client = client
		self.uuid = uuid
		self.characteristic = characteristic

		# Protected members
		self._handleNo = handleNo

	# Public methods

	def read(self):
		"""Blocking read of the descriptor.

		Returns: 
			A bytearray
		Raises:
			ClientError on failure
		"""

		req = bytearray([att.OP_READ_REQ])
		req += _handle_to_bytearray(self._handleNo)
		resp = self.client._new_transaction(req)
		if resp is None:
			raise ClientError("no response")
		elif resp[0] == att.OP_READ_RESP:
			return resp[1:]
		elif resp[0] == att.OP_ERROR and len(resp) == att_pdu.ERROR_PDU_LEN:
			raise ClientError("read failed - %s" % att.ecodeLookup(resp[4]))
		else:
			raise ClientError("unexpected - %s" % att.opcodeLookup(resp[0]))

	def write(self, value):
		"""Blocking write of the descriptor.

		Args:
			value : a bytearray of length at most send MTU - 3
		Raises:
			ClientError on failure
		"""

		assert type(value) is bytearray

		req = bytearray([att.OP_WRITE_REQ])
		req += _handle_to_bytearray(self._handleNo)
		req += value

		resp = self.client._new_transaction(req)
		if resp is None:
			raise ClientError("no response")
		elif resp[0] == att.OP_WRITE_RESP:
			return resp[1:]
		elif resp[0] == att.OP_ERROR and len(resp) == att_pdu.ERROR_PDU_LEN:
			raise ClientError("write failed - %s" % att.ecodeLookup(resp[4]))
		else:
			raise ClientError("unexpected - %s" % att.opcodeLookup(resp[0]))

	# Protected methods

	def __str__(self):
		return "Descriptor - %s" % str(self.uuid)

class GattClient:
	def __init__(self, socket, onDisconnect=None):
		"""Make a GATT client

		Args:
			socket : a ManagedSocket instance
			onDisconnect : callback on disconnect
		"""
		assert isinstance(socket, ManagedSocket)

		# Public members
		self.services = []

		# Protected members
		self._socket = socket
		self._socket._set_client(self)

		self._disconnected = False
		self._onDisconnect = onDisconnect

		self._transactionLock = threading.Lock()
		self._responseEvent = threading.Event()
		
		self._currentRequest = None
		self._currentResponse = None

		self._valHandles = {}

		self._serviceHandles = {}
		
	# Public methods

	def discoverServices(self, uuid=None):
		"""Find services on the server.

		Notes: if uuid is None, all services are discovered. Any previously 
			discovered services are invalidated.
		Args:
			uuid : type of service to discover
		Returns:
			A list of services."""
		if uuid and not isinstance(uuid, UUID):
			uuid = UUID(uuid)

		if uuid is None:
			return self.__discover_all_services()
		else:
			return self.__discover_services_by_uuid(uuid)

	# Protected and private methods

	def _add_val_handle(self, characteristic, valHandleNo):
		self._valHandles[valHandleNo] = characteristic

	def _handle_disconnect(self, err=None):
		if self._onDisconnect is not None:
			self._onDisconnect(err)
			self._disconnected = True
			self._responseEvent.set()

	def _new_transaction(self, req):
		assert type(req) is bytearray
		assert att.isRequest(req[0])

		self._transactionLock.acquire()
		assert self._currentRequest is None
		assert self._currentResponse is None
		
		try:
			if self._disconnected:
				return None

			self._currentRequest = req
			self._socket._send(req)
			if self._responseEvent.wait(60):
				return self._currentResponse
		finally:
			self._currentRequest = None
			self._currentResponse = None
			self._responseEvent.clear()
			self._transactionLock.release()

		return None

	def _handle_packet(self, pdu):
		op = pdu[0]
		if (att.isResponse(op) or (op == att.OP_ERROR 
			and self._currentRequest and self._currentResponse is None 
			and pdu[1] == self._currentRequest[0])):

			self._currentResponse = pdu
			self._responseEvent.set()
			return None

		if op == att.OP_HANDLE_NOTIFY or op == att.OP_HANDLE_IND:
			if len(pdu) < 3:
				return att_pdu.new_error_resp(op, 0, att.ECODE_INVALID_PDU)
			
			_, handleNo, value = att_pdu.parse_notify_indicate(pdu)

			if handleNo not in self._valHandles:
				return None

			charac = self._valHandles[handleNo]
			if charac._subscribeCallback is not None:
				charac._subscribeCallback(value)

			if op == att.OP_HANDLE_IND:
				return bytearray([att.OP_HANDLE_CNF])
			
		return None

	def __discover_all_services(self):
		startHandle = 1
		endHandle = 0xFFFF
		primSvcUuid = UUID(gatt.PRIM_SVC_UUID)

		services = []
		serviceHandles = {}
		currHandle = startHandle
		while True:
			req = att_pdu.new_read_by_group_req(currHandle, endHandle, 
				primSvcUuid)

			resp = self._new_transaction(req)
			if not resp:
				raise ClientError("transaction failed")

			if resp[0] == att.OP_READ_BY_GROUP_RESP:
				attDataLen = resp[1]
				idx = 2

				endGroup = currHandle # not needed
				while idx < len(resp) and idx + attDataLen <= len(resp): 
					handleNo = att_pdu.unpack_handle(resp, idx)
					endGroup = att_pdu.unpack_handle(resp, idx + 2)
					uuid = UUID(resp[idx+4:idx+attDataLen], reverse=True)

					service = _ClientService(self, uuid, handleNo, endGroup)
					services.append(service)
					serviceHandles[handleNo] = service

					idx += attDataLen

				currHandle = endGroup + 1
				if currHandle >= endHandle:
					break

			elif (resp[0] == att.OP_ERROR and len(resp) == att_pdu.ERROR_PDU_LEN
				and resp[1] == att.OP_READ_BY_GROUP_REQ
				and resp[4] == att.ECODE_ATTR_NOT_FOUND):
				break

			elif (resp[0] == att.OP_ERROR and len(resp) == att_pdu.ERROR_PDU_LEN 
				and resp[1] == att.OP_READ_BY_GROUP_REQ):
				raise ClientError("error - %s" % att.ecodeLookup(resp[4]))

			else:
				raise ClientError("unexpected - %s" % att.opcodeLookup(resp[0]))

		self._serviceHandles = serviceHandles
		self.services = services
		return services

	def __discover_service_by_uuid(self, uuid):
		startHandle = 1
		endHandle = 0xFFFF
		primSvcUuid = UUID(gatt.PRIM_SVC_UUID)

		services = []
		newServices = []
		serviceHandles = {}
		currHandle = startHandle

		while True:
			req = att_pdu.new_find_by_type_value_req(currHandle, endHandle, 
				primSvcUuid, uuid.raw()[::-1])

			resp = self._new_transaction(req)
			if not resp:
				raise ClientError("transaction failed")

			if resp[0] == att.OP_FIND_BY_TYPE_REQ:
				idx = 2

				endGroup = currHandle # not needed
				while idx < len(resp) and idx + 4 <= len(resp): 
					handleNo = att_pdu.unpack_handle(resp, idx)
					endGroup = att_pdu.unpack_handle(resp, idx + 2)

					if handleNo in self._serviceHandles:
						service = self._serviceHandles[handleNo]
					else:
						service = _ClientService(self, uuid, handleNo, endGroup)
						serviceHandles[handleNo] = service
						newServices.append(service)
					services.append(service)

					idx += 4

				currHandle = endGroup + 1
				if currHandle >= endHandle:
					break

			elif (resp[0] == att.OP_ERROR and len(resp) == att_pdu.ERROR_PDU_LEN 
				and resp[1] == att.OP_FIND_BY_TYPE_REQ
				and resp[4] == att.ECODE_ATTR_NOT_FOUND):
				break

			elif (resp[0] == att.OP_ERROR and len(resp) == att_pdu.ERROR_PDU_LEN 
				and resp[1] == att.OP_FIND_BY_TYPE_REQ):
				raise ClientError("error - %s" % att.ecodeLookup(resp[4]))

			else:
				raise ClientError("unexpected - %s" % att.opcodeLookup(resp[0]))

		self.services.extend(newServices)
		self.services.sort(key=lambda x: x._handleNo)
		self._serviceHandles.update(serviceHandles)

		return services

	def __str__(self, indent=2):
		tmp = []
		for service in self.services:
			tmp.append(str(service))
			for charac in service.characteristics:
				tmp.append(" " * indent + str(charac))
				if charac._cccd:
					tmp.append(" " * indent * 2 + str(charac._cccd))
				for descriptor in charac.descriptors:
					tmp.append(" " * indent * 2 + str(descriptor))
		return "\n".join(tmp)
