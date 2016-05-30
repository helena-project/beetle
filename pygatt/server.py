import lib.att as att
import lib.att_pdu as att_pdu
import lib.gatt as gatt
from lib.uuid import UUID

class ServerError(Exception):
	"""Standard error type raised by GattServer.
	
	Note: may safely be raised by callbacks
	"""
	def __init__(self, msg):
		self.msg = msg

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

	def _setReadCallback(self, cb):
		def __wrapper():
			return self.__convert_to_bytearray(cb())
		self._read_callback = __wrapper

	def _setReadValue(self, value):
		def __wrapper():
			return self.__convert_to_bytearray(value)
		self._read_callback = __wrapper

	def _setWriteCallback(self, cb):
		self._write_callback = cb

	def _getHandleAsBytearray(self):
		arr = bytearray(2)
		arr[0] = self._no & 0xFF
		arr[1] = (self._no >> 8) & 0xFF
		return arr

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
		self._handle._setReadValue(uuid.raw()[::-1])

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

			self._cccd._handle._setWriteCallback(cccd_cb)
			self._cccd.write(bytearray([0, 0]))
		else:
			self._cccd = None

		if staticValue:
			self._valHandle._setReadValue(staticValue) 

		def read_cb():
			"""Properties may change."""
			handleValue = bytearray([self._properties])
			handleValue += self._valHandle._getHandleAsBytearray()
			handleValue += self.uuid.raw()[::-1]
			return handleValue	
		self._handle._setReadCallback(read_cb)

	# Public methods

	def setReadCallback(self, cb):
		"""Allow reads and sets a callback to handle client reads.

		Args: 
			cb : a function that returns a bytearray
		"""
		self._properties |= gatt.CHARAC_PROP_READ
		self._valHandle._setReadCallback(cb)

	def setReadValue(self, value):
		"""Allow reads and sets a static value.
		
		Args: 
			value : a bytearray, should be shorter than the the send MTU - 3
		"""
		self._properties |= gatt.CHARAC_PROP_READ
		self._valHandle._setReadValue(value)

	def setWriteCallback(self, cb):
		"""Allow writes and sets a callback to handle client writes

		Args:
			cb : a function that takes a bytearray as argument
		"""
		self._properties |= (gatt.CHARAC_PROP_WRITE | gatt.CHARAC_PROP_WRITE_NR)
		self._valHandle._setWriteCallback(cb)

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
			pdu += self._valHandle._getHandleAsBytearray()
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
			pdu += self._valHandle._getHandleAsBytearray()
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
	def __init__(self, server, uuid):
		assert isinstance(server, GattServer)
		assert isinstance(uuid, UUID)

		# Public members
		self.server = server
		self.uuid = uuid
		self.characteristic = server.services[-1].characteristics[-1]

		self.characteristic._add_descriptor(self)

		# Protected members
		self._handle = _ServerHandle(server, self.characteristic, uuid)
		self._value = None

	def write(self, value):
		"""Write a value to the descriptor.
		
		Args:
			value : a bytearray with length not exceeding send MTU - 3
		"""
		assert type(value) is bytearray
		self._value = value
		self._handle._setReadValue(self._value)

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

		# Public members
		self.services = []

		# Protected members
		self._socket = socket
		self._socket._setServer(self)

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

		return _ServerCharacteristic(self, uuid, value, allowNotify, allowIndicate)
	
	def addDescriptor(self, uuid):
		"""Add a descriptor to the last characteristic

		Args:
			uuid : the type of the descriptor
		Returns:
			The newly added characteristic
		"""
		if not isinstance(uuid, UUID):
			uuid = UUID(uuid)

		return _ServerDescriptor(self, uuid)

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
			resp += handle._getHandleAsBytearray()
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
			resp += handle._getHandleAsBytearray()
			resp += handle._owner._get_end_group_handle()._getHandleAsBytearray()
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
			resp += handle._getHandleAsBytearray()
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
			resp += handle._getHandleAsBytearray()
			resp += handle._owner._get_end_group_handle()._getHandleAsBytearray()
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