import lib.att as att
import lib.att_pdu as att_pdu
import lib.gatt as gatt
from lib.uuid import UUID

class _Handle:
	def __init__(self, server, owner, uuid):
		assert type(server) is GattServer
		assert type(uuid) is UUID
		assert owner
		assert owner._get_end_group_handle is not None
		assert owner._get_start_group_handle is not None

		server._add_handle(self)

		self.no = server._alloc_handle()
		self.uuid = uuid
		self.readCallback = None
		self.writeCallback = None
		self.owner = owner

	def read(self):
		try:
			return self.readCallback()
		except:
			return None

	def write(self):
		try:
			self.writeCallback()
		except:
			pass

	def setReadCallback(self, cb):
		self.readCallback = cb

	def setReadValue(self, value):
		self.readCallback = lambda: value

	def setWriteCallback(self, cb):
		self.writeCallback = cb

	def getHandleAsBytearray(self):
		arr = bytearray(2)
		arr[0] = self.no & 0xFF
		arr[1] = (self.no >> 8) & 0xFF
		return arr

class _Service:
	def __init__(self, server, uuid):
		assert type(server) is GattServer
		assert type(uuid) is UUID

		self.uuid = uuid
		self.characteristics = []

		self._handle = _Handle(server, self, UUID(gatt.PRIM_SVC_UUID))
		self._handle.setReadValue(uuid.raw())

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
			self._cccd = _Descriptor(server, 
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

		self._handle = _Handle(server, self, UUID(gatt.CHARAC_UUID))
		self._valHandle = _Handle(server, self, uuid)

		def read_cb():
			handleValue = bytearray([self.properties])
			handleValue += self._valHandle.getHandleAsBytearray()
			handleValue += uuid.raw()
			return handleValue	
		self._handle.setReadCallback(read_cb)

	def setReadCallback(self, cb):
		self.properties |= gatt.CHARAC_PROP_READ
		self._valHandle.setReadCallback(cb)

	def setWriteCallback(self, cb):
		self.properties |= (gatt.CHARAC_PROP_WRITE | gatt.CHARAC_PROP_WRITE_NR)
		self._valHandle.setWriteCallback(cb)

	def sendNotify(self, value):
		assert type(value) is bytearray
		assert self._cccd != None
		
		cccdValue = self._cccd.read()
		if cccdValue is not None and cccdValue[0] == 1:
			pdu = bytearray([att.OP_HANDLE_NOTIFY])
			pdu += self._valHandle.getHandleAsBytearray()
			pdu += value
			self.server._sock._send(pdu)

	def sendIndicate(self, value, cb):
		assert type(value) is bytearray
		assert self._cccd != None

		cccdValue = self._cccd.read()
		if cccdValue is not None and cccdValue[1] == 1:
			self.server._indicateQueue.append(cb)
			pdu = bytearray([att.OP_HANDLE_NOTIFY])
			pdu += self._valHandle.getHandleAsBytearray()
			pdu += value
			self.server._sock._send(pdu)
			return True
		else:
			return False

	def _add_descriptor(self, descriptor):
		self.descriptors.append(descriptor)

	def _get_start_group_handle(self):
		return self._handle

	def _get_end_group_handle(self):
		return self.descriptors[-1]._handle if self.descriptors else \
		self._valHandle

	def __len__(self):
		return len(self.descriptors) + 1

	def __str__(self):
		return "Characteristic - %s" % str(self.uuid)

class _Descriptor:
	def __init__(self, server, uuid):
		assert type(uuid) is UUID
		assert type(server) is GattServer

		self.uuid = uuid
		self.char = server.services[-1].chars[-1]
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
	def __init__(self, socket, mtu=23, onDisconnect=None):
		self._socket = socket
		self._socket._setServer(self)

		self._handles = [None,]
		self._currHandleOfs = 0
		
		self.mtu = mtu
		self.services = []

		self._indicateQueue = []

		self._onDisconnect = onDisconnect

	def addService(self, uuid):
		service = _Service(self, uuid)
		self.services.append(service)
		return service

	def addCharacteristic(self, uuid):
		return _Characteristic(server, uuid)
	
	def addDescriptor(self):
		return _Descriptor(server, uuid)

	def __is_valid_handle(self, handleNo):
		return handleNo != 0 and handleNo <len(self._handles)

	def _alloc_handle(self):
		currHandleOfs += 1
		return currHandleOfs

	def _add_handle(self, handle):
		self.handles.append(handle)

	def _handle_disconnect(self, err=None):
		if self_.onDisconnect is not None:
			 self._onDisconnect(err)

	UNSUPPORTED_REQ = set([
		att.OP_READ_BLOB_REQ, att.OP_READ_MULTI_REQ, 
		att.OP_PREP_WRITE_REQ, att.OP_EXEC_WRITE_REQ, 
		att.OP_SIGNED_WRITE_CMD])

	def _handle_packet(self, pdu):
		op = pdu[0]
		if op == att.OP_MTU_REQ:
			resp = bytearray(3)
			resp[0] = att.OP_MTU_RESP
			resp[1] = self.mtu & 0xFF
			resp[2] = (self.mtu >> 8) & 0xFF
			return resp

		elif op == att.OP_FIND_INFO_REQ:
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
					respFmt = len(handle.uuid)
					resp.append(1 if respFmt == 2 else 2)
				if len(handle.uuid) != respFmt:
					break
				if len(resp) + 2 + respFmt > self.mtu:
					break
				resp += handle.getHandleAsBytearray()
				resp += handle.uuid.raw()
				respCount += 1

			if respCount == 0:
				return att_pdu.new_error_resp(op, startHandle, \
					att.ECODE_ATTR_NOT_FOUND)
			else:
				return resp

		elif op == att.OP_FIND_BY_TYPE_REQ:
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
				if handle.uuid != uuid:
					continue
				if len(resp) + 4 > self.mtu:
					break
				resp += handle.getHandleAsBytearray()
				resp += handle.owner._get_end_group_handle().getHandleAsBytearray()
				respCount += 1

			if respCount == 0:
				return att_pdu.new_error_resp(op, startHandle, \
					att.ECODE_ATTR_NOT_FOUND)
			else:
				return resp

		elif op == att.OP_READ_BY_TYPE_REQ:
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
				if handle.uuid != uuid:
					continue

				valRead = handle.read()
				if valRead is None:
					return att_pdu.new_error_resp(op, startHandle, 
						att.ECODE_READ_NOT_PERM)

				assert type(valRead) is bytearray
				if not valLen:
					assert len(valLen) <= 0xFF - 2
					valLen = len(valRead)
					resp[1] = valLen + 2
				if valLen != len(valRead):
					break
				if len(resp) + 2 + valLen > self.mtu:
					break
				resp += handle.getHandleAsBytearray()
				resp += valRead
				respCount += 1

			if respCount == 0:
				return att_pdu.new_error_resp(op, startHandle, \
					att.ECODE_ATTR_NOT_FOUND)
			else:
				return resp

		elif op == att.OP_READ_BY_GROUP_REQ:
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
				if handle.uuid != uuid:
					continue

				valRead = handle.read()
				if valRead is None:
					return att_pdu.new_error_resp(op, startHandle, 
						att.ECODE_READ_NOT_PERM)

				assert type(valRead) is bytearray
				if not valLen:
					assert len(valLen) <= 0xFF - 4 
					valLen = len(valRead)
					resp[1] = valLen + 4
				if valLen != len(valRead):
					break
				if len(resp) + 4 + valLen > self.mtu:
					break
				resp += handle.getHandleAsBytearray()
				resp += handle.owner._get_end_group_handle().getHandleAsBytearray()
				resp += valRead
				respCount += 1

			if respCount == 0:
				return att_pdu.new_error_resp(op, startHandle, \
					att.ECODE_ATTR_NOT_FOUND)
			else:
				return resp

		elif op == att.OP_READ_REQ:
			if len(pdu) < 3:
				return  att_pdu.new_error_resp(op, 0, att.ECODE_INVALID_PDU)

			_, handleNo = att_pdu.parse_read_req(pdu)

			if not self.__is_valid_handle(handleNo):
				return att_pdu.new_error_resp(op, handleNo, 
					att.ECODE_INVALID_HANDLE)
			
			if self.server._handles[handleNo].readCallback:
				try:
					self.server._handles[handleNo].readCallback(value)
					resp = bytearray([att.OP_READ_RESP])
					resp += self.server._handles[handleNo].readCallback()
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

			if self.server._handles[handleNo].writeCallback:
				try:
					self.server._handles[handleNo].writeCallback(value)
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