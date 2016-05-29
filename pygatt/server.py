import lib.att as att
import lib.att_pdu as att_pdu
import lib.gatt as gatt
from lib.uuid import UUID

class ServerError(Exception):
	def __init__(self, arg):
		self.msg = arg

class _Handle:
	def __init__(self, server, owner, uuid):
		assert server.__class__ is GattServer
		assert owner is not None
		assert owner._get_end_group_handle is not None
		assert owner._get_start_group_handle is not None

		if uuid.__class__ is not UUID:
			uuid = UUID(uuid)

		server._add_handle(self)

		self._no = server._alloc_handle()
		self._uuid = uuid
		self._read_callback = None
		self._write_callback = None
		self._owner = owner

	def read(self):
		try:
			return self._read_callback()
		except ServerError, err:
			return None

	def write(self, value):
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

	def setReadCallback(self, cb):
		def __wrapper():
			return self.__convert_to_bytearray(cb())
		self._read_callback = __wrapper

	def setReadValue(self, value):
		def __wrapper():
			return self.__convert_to_bytearray(value)
		self._read_callback = __wrapper

	def setWriteCallback(self, cb):
		self._write_callback = cb

	def getHandleAsBytearray(self):
		arr = bytearray(2)
		arr[0] = self._no & 0xFF
		arr[1] = (self._no >> 8) & 0xFF
		return arr

	def __str__(self):
		return str(self._owner)

class _Service:
	def __init__(self, server, uuid):
		assert server.__class__ is GattServer

		if uuid.__class__ is not UUID:
			uuid = UUID(uuid)

		self.uuid = uuid
		self.characteristics = []

		self._handle = _Handle(server, self, UUID(gatt.PRIM_SVC_UUID))

		# reversed by convention
		self._handle.setReadValue(uuid.raw()[::-1])

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

class _Characteristic:
	def __init__(self, server, uuid, staticValue=None, allowNotify=False, 
		allowIndicate=False):
		assert server.__class__ is GattServer

		if uuid.__class__ is not UUID:
			uuid = UUID(uuid)

		self.server = server
		self.uuid = uuid
		self.descriptors = []

		self._properties = 0
		if staticValue:
			self._properties |= gatt.CHARAC_PROP_READ
		if allowNotify:
			self._properties |= gatt.CHARAC_PROP_NOTIFY	
		if allowIndicate:
			self._properties |= gatt.CHARAC_PROP_IND

		self.service = server.services[-1]
		self.service._add_characteristic(self)

		self._has_subscriber = False
		self._subscribe_callback = lambda: None
		self._unsubscribe_callback = lambda: None

		self._handle = _Handle(server, self, UUID(gatt.CHARAC_UUID))
		self._valHandle = _Handle(server, self, self.uuid)

		if allowNotify or allowIndicate:
			self._cccd = _Descriptor(server, 
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

			self._cccd._handle.setWriteCallback(cccd_cb)
			self._cccd.write(bytearray([0, 0]))
		else:
			self._cccd = None

		if staticValue:
			self._valHandle.setReadValue(staticValue) 

		def read_cb():
			"""
			Properties may change.
			"""
			handleValue = bytearray([self._properties])
			handleValue += self._valHandle.getHandleAsBytearray()
			handleValue += self.uuid.raw()[::-1]
			return handleValue	
		self._handle.setReadCallback(read_cb)

	def setReadCallback(self, cb):
		self._properties |= gatt.CHARAC_PROP_READ
		self._valHandle.setReadCallback(cb)

	def setReadValue(self, value):
		self._properties |= gatt.CHARAC_PROP_READ
		self._valHandle.setReadValue(value)

	def setWriteCallback(self, cb):
		self._properties |= (gatt.CHARAC_PROP_WRITE | gatt.CHARAC_PROP_WRITE_NR)
		self._valHandle.setWriteCallback(cb)

	def setSubscribeCallback(self, cb):
		if self._cccd is None:
			raise ServerError("subscriptions not allowed")
		self._subscribe_callback = cb

	def setUnsubscribeCallback(self, cb):
		if self._cccd is None:
			raise ServerError("subscriptions not allowed")
		self._unsubscribe_callback = cb

	def sendNotify(self, value):
		assert type(value) is bytearray
		assert self._cccd != None
		
		cccdValue = self._cccd.read()
		if cccdValue is not None and cccdValue[0] == 1:
			pdu = bytearray([att.OP_HANDLE_NOTIFY])
			pdu += self._valHandle.getHandleAsBytearray()
			pdu += value
			self.server._socket._send(pdu)

	def sendIndicate(self, value, cb):
		assert type(value) is bytearray
		assert self._cccd != None

		cccdValue = self._cccd.read()
		if cccdValue is not None and cccdValue[1] == 1:
			self.server._indicateQueue.append(cb)
			pdu = bytearray([att.OP_HANDLE_NOTIFY])
			pdu += self._valHandle.getHandleAsBytearray()
			pdu += value
			self.server._socket._send(pdu)
			return True
		else:
			return False

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

class _Descriptor:
	def __init__(self, server, uuid):
		assert server.__class__ is GattServer

		if uuid.__class__ is not UUID:
			uuid = UUID(uuid)

		self.uuid = uuid
		self.char = server.services[-1].characteristics[-1]
		self.char._add_descriptor(self)

		self._handle = _Handle(server, self.char, uuid)
		self._value = None

	def write(self, value):
		assert type(value) is bytearray
		self._value = value
		self._handle.setReadValue(self._value)

	def read(self):
		return self._value

	def __str__(self):
		return "Descriptor - %s" % str(self.uuid)

class GattServer:
	def __init__(self, socket, onDisconnect=None):
		self._socket = socket
		self._socket._setServer(self)

		self._handles = [None,]
		self._currHandleOfs = 0
		
		self.services = []

		self._indicateQueue = []

		self._onDisconnect = onDisconnect

	def addGapService(self, deviceName):
		assert len(self._handles) == 1
		assert type(deviceName) is str

		self.addService(gatt.GAP_SERVICE_UUID)
		self.addCharacteristic(gatt.GAP_CHARAC_DEVICE_NAME_UUID, deviceName)

	def addService(self, uuid):
		service = _Service(self, uuid)
		self.services.append(service)
		return service

	def addCharacteristic(self, uuid, value=None, allowNotify=False, 
		allowIndicate=False):
		return _Characteristic(self, uuid, value, allowNotify, allowIndicate)
	
	def addDescriptor(self):
		return _Descriptor(self, uuid)

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
			resp += handle.getHandleAsBytearray()
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
			resp += handle.getHandleAsBytearray()
			resp += handle._owner._get_end_group_handle().getHandleAsBytearray()
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

			valRead = handle.read()
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
			resp += handle.getHandleAsBytearray()
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

			valRead = handle.read()
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
			resp += handle.getHandleAsBytearray()
			resp += handle._owner._get_end_group_handle().getHandleAsBytearray()
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

	def __str__(self):
		tmp = []
		for service in self.services:
			tmp.append(str(service))
			for charac in service.characteristics:
				tmp.append(str(charac))
				for descripor in charac.descriptors:
					tmp.append(str(descriptor))
		return "\n".join(tmp)