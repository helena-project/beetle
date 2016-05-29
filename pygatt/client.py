import threading

import lib.att as att
import lib.att_pdu as att_pdu
import lib.gatt as gatt
from lib.uuid import UUID

def __handle_to_bytearray(handle):
	assert handle <= 0xFFFF
	assert handle >= 0
	return bytearray([handle & 0xFF, (handle >> 8) & 0xFF])

class ClientError(Exception):
    def __init__(self, arg):
        self.msg = arg

class _Service:
	def __init__(self, client, uuid, handleNo, endGroup):
		assert uuid.__class__ is UUID
		assert client.__class__ is GattClient
		assert type(handleNo) is int
		assert type(endGroup) is int

		self.client = client
		self.uuid = uuid

		self._handleNo = handleNo
		self._endGroup = endGroup 

		self.characteristics = []
		self._characteristicHandles = {}

	def discoverCharacteristics(self, uuid=None):
		if uuid is None:
			return self._discoverAllCharacteristics()
		else:
			raise NotImplementedError

	def _discoverAllCharacteristics(self):
		startHandle = self._handleNo + 1
		endHandle = self._endGroup
		characUuid = UUID(gatt.CHARAC_UUID)

		characs = []
		characHandles = {}
		currHandle = startHandle
		while True:
			req = att_pdu.new_read_by_type_req(startHandle, endHandle, 
				characUuid)

			resp = self._new_transaction(req)
			if not resp:
				raise ClientError("transaction failed")

			if resp[0] == att.OP_READ_BY_TYPE_RESP:
				attDataLen = resp[1]
				idx = 2

				handleNo = currHandle # not needed
				while idx < len(resp) and idx + attDataLen <= len(resp): 
					handleNo = resp[idx] + (resp[idx+1] >> 8)
					properties = resp[idx+2]
					valHandleNo = resp[idx+3] + (resp[idx+4] >> 8)
					uuid = UUID(resp[idx+5:idx+attDataLen])

					charac = _Characteristic(self.client, self, uuid, handleNo, 
						properties, valHandleNo)
					characs.append(charac)
					characHandles[handleNo] = characs

					idx += attDataLen

				currHandle = handleNo + 1
				if currHandle >= endHandle:
					break

			elif (resp[0] == att.OP_ERROR and resp[1] == att.OP_READ_BY_TYPE_REQ
				and resp[4] == att.ECODE_ATTR_NOT_FOUND):
				break

			elif (resp[0] == att.OP_ERROR and resp[1] == att.OP_READ_BY_TYPE_REQ
				and resp[4] == att.ECODE_REQ_NOT_SUPP):
				raise ClientError("not supported")

			else:
				raise ClientError("error: %02x" % resp[0])

		for i, charac in enumerate(characs):
			if i + 1 < len(characs):
				charac._setEndGroup(characs[i+1]._handleNo-1)
			else:
				charac._setEndGroup(self._endGroup)

		self._characteristicHandles = characHandles
		self.characteristics = characs
		return characs

	def __len__(self):
		return len(characteristics)

	def __str__(self):
		return "Service - %s" % str(self.uuid)

class _Characteristic:
	def __init__(self, client, service, uuid, handleNo, properties, 
		valHandleNo, endGroup=None):
		assert uuid.__class__ is UUID
		assert client.__class__ is GattClient
		assert type(service) is _Service
		assert type(handleNo) is int
		assert type(valHandleNo) is int
		assert type(properties) is int

		self.client = client
		self.uuid = uuid
		self.service = service
	 	self.permissions = set()
		
		if (properties & gatt.CHARAC_PROP_READ) != 0:
			permissions.add('r')
		if (properties & gatt.CHARAC_PROP_WRITE) != 0:
			permissions.add('w')
		if (properties & gatt.CHARAC_PROP_IND) != 0:
			permissions.add('i')
		if (properties & gatt.CHARAC_PROP_NOTIFY) != 0:
			permissions.add('n')

		self.client._add_val_handle(self, valHandleNo)

		self._handleNo = handleNo
		self._valHandleNo = valHandleNo
		self._endGroup = endGroup

		self._subscribeCallback = None

		self._cccd = None
		self.descriptors = []

	def _setEndGroup(self, endGroup):
		self._endGroup = endGroup

	def discoverDescriptors(self):
		assert self._endGroup is not None
		
		startHandle = self._handleNo + 1
		endHandle = self._endGroup
		
		descriptors = []

		cccdUuid = UUID(gatt.CLIENT_CHARAC_CFG_UUID)
		cccd = None
		
		currHandle = startHandle
		while True:
			req = att_pdu.new_find_info_req(startHandle, endHandle)

			resp = self._new_transaction(req)
			if not resp:
				raise ClientError("transaction failed")

			if resp[0] == att.OP_FIND_INFO_RESP:
				attDataLen = 4 if resp[1] == att.FIND_INFO_RESP_FMT_16BIT else 18
				idx = 2

				handleNo = currHandle # not needed
				while idx < len(resp) and idx + attDataLen <= len(resp): 
					handleNo = resp[idx] + (resp[idx+1] >> 8)
					uuid = UUID(resp[idx+2:idx+attDataLen])

					if handleNo == self._valHandleNo:
						idx += attDataLen
						continue

					descriptor = _Descriptor(self.client, self, uuid, handleNo)
					if uuid == cccdUuid:
						cccd = descriptor
					else:
						descriptors.append(descriptor)

					idx += attDataLen

				currHandle = handleNo + 1
				if currHandle >= endHandle:
					break

			elif (resp[0] == att.OP_ERROR and resp[1] == att.OP_FIND_INFO_REQ
				and resp[4] == att.ECODE_ATTR_NOT_FOUND):
				break

			elif (resp[0] == att.OP_ERROR and resp[1] == att.OP_FIND_INFO_REQ
				and resp[4] == att.ECODE_REQ_NOT_SUPP):
				raise ClientError("not supported")

			else:
				raise ClientError("error: %02x" % resp[0])

		self.descriptors = descriptors
		self._cccd = cccd
		return descriptors

	def subscribe(self, cb):
		if "i" not in self.permissions and "n" not in self.permissions:
			raise ClientError("subscription not permitted")

		if self._cccd is None:
			raise ClientError("need to discover cccd first!")

		self._subscribeCallback = cb

		value = bytearray(2)
		value[0] = 1 if "i" in self.permissions else 0
		value[1] = 1 if "n" in self.permissions else 0

		self._cccd.write(value)

	def unsubscribe(self):
		self._subscribeCallback = None

		if self._cccd is not None:
			self._cccd.write( ytearray([0,0]))

	def read(self, value):
		if "r" not in self.permissions:
			raise ClientError("read not permitted")

		req = bytearray([att.OP_READ_REQ])
		req += __handle_to_bytearray(self._valHandleNo)
		resp = client._new_transaction(req)
		if resp is None:
			raise ClientError("no response")
		elif resp[0] == att.OP_READ_RESP:
			return resp[1:]
		elif resp[0] == att.OP_ERROR and len(resp) == 5:
			raise ClientError("error: %02x" % resp[4])
		else:
			raise ClientError("unexpected packet")

	def write(self, value):
		assert type(value) is bytearray
		if "w" not in self.permissions:
			raise ClientError("write not permitted")

		req = bytearray([att.OP_WRITE_REQ])
		req += __handle_to_bytearray(self._valHandleNo)
		req += value

		resp = client._new_transaction(req)
		if resp is None:
			raise ClientError("no response")
		elif resp[0] == att.OP_WRITE_RESP:
			return resp[1:]
		elif resp[0] == att.OP_ERROR and len(resp) == 5:
			raise ClientError("error: %02x" % resp[4])
		else:
			raise ClientError("unexpected packet")

	def __len__(self):
		return len(descriptors)

	def __str__(self):
		return "Characteristic - %s" % str(self.uuid)

class _Descriptor:
	def __init__(self, client, characteristic, uuid, handleNo):
		assert uuid.__class__ is UUID
		assert client.__class__ is GattClient
		assert type(characteristic) is _Characteristic
		assert type(handleNo) is int

		self.client = client
		self.uuid = uuid
		self.characteristic = characteristic

		self._handleNo = handleNo

	def read(self):
		req = bytearray([att.OP_READ_REQ])
		req += __handle_to_bytearray(self._handleNo)
		resp = client._new_transaction(req)
		if resp is None:
			raise ClientError("no response")
		elif resp[0] == att.OP_READ_RESP:
			return resp[1:]
		elif resp[0] == att.OP_ERROR and len(resp) == 5:
			raise ClientError("error: %02x" % resp[4])
		else:
			raise ClientError("unexpected packet")

	def write(self, value):
		assert type(value) is bytearray

		req = bytearray([att.OP_WRITE_REQ])
		req += __handle_to_bytearray(self._handleNo)
		req += value

		resp = client._new_transaction(req)
		if resp is None:
			raise ClientError("no response")
		elif resp[0] == att.OP_WRITE_RESP:
			return resp[1:]
		elif resp[0] == att.OP_ERROR and len(resp) == 5:
			raise ClientError("error: %02x" % resp[4])
		else:
			raise ClientError("unexpected packet")

	def __str__(self):
		return "Descriptor - %s" % str(self.uuid)

class GattClient:
	def __init__(self, socket, onDisconnect=None):
		self._socket = socket
		self._socket._setClient(self)

		self._disconnected = False
		self._onDisconnect = onDisconnect

		self._transactionLock = threading.Lock()
		self._responseSema = threading.BoundedSemaphore(1)
		self._responseSema.acquire() # decrement to 0
		
		self._currentRequest = None
		self._currentResponse = None

		self._valHandles = {}

		self._serviceHandles = {}
		self.services = []
		

	def discoverServices(self, uuid=None):
		if uuid is None:
			return self._discoverAllServices()
		else:
			raise NotImplementedError

	def _add_val_handle(self, characteristic, valHandleNo):
		self._valHandles[valHandleNo] = characteristic

	def _handle_disconnect(self, err=None):
		if self._onDisconnect is not None:
			self._onDisconnect(err)
			self._disconnected = True
			try:
				self._responseSema.release()
			except ValueError:
				pass

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
			self._responseSema.acquire()

			return self._currentResponse
		finally:
			self._currentRequest = None
			self._currentResponse = None
			self._transactionLock.release()

		return None

	def _handle_packet(self, pdu):
		op = pdu[0]
		if (att.isResponse(op) or (op == att.OP_ERROR 
			and self._currentRequest and self._currentResponse is None 
			and pdu[1] == self._currentRequest[0])):

			self._currentResponse = pdu
			try:
				self._responseSema.release()
			except ValueError:
				pass
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

	def _discoverAllServices(self):
		startHandle = 1
		endHandle = 0xFFFF
		primSvcUuid = UUID(gatt.PRIM_SVC_UUID)

		services = []
		serviceHandles = {}
		currHandle = startHandle
		while True:
			req = att_pdu.new_read_by_group_req(startHandle, endHandle, 
				primSvcUuid)

			resp = self._new_transaction(req)
			if not resp:
				raise ClientError("transaction failed")

			if resp[0] == att.OP_READ_BY_GROUP_REQ:
				attDataLen = resp[1]
				idx = 2

				endGroup = currHandle # not needed
				while idx < len(resp) and idx + attDataLen <= len(resp): 
					handleNo = resp[idx] + (resp[idx+1] >> 8)
					endGroup = resp[idx+2] + (resp[idx+3] >> 8)
					uuid = UUID(resp[idx+4:idx+attDataLen])

					service = _Service(self, uuid, handleNo, endGroup)
					services.append(service)
					serviceHandles[handleNo] = service

					idx += attDataLen

				currHandle = endGroup + 1
				if currHandle >= endHandle:
					break

			elif (resp[0] == att.OP_ERROR and resp[1] == att.OP_READ_BY_GROUP_REQ
				and resp[4] == att.ECODE_ATTR_NOT_FOUND):
				break

			elif (resp[0] == att.OP_ERROR and resp[1] == att.OP_READ_BY_GROUP_REQ
				and resp[4] == att.ECODE_REQ_NOT_SUPP):
				raise ClientError("not supported")

			else:
				raise ClientError("error: %02x" % resp[0])

		self._serviceHandles = serviceHandles
		self.services = services
		return services
