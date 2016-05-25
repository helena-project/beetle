import threading

import lib.att as att
import lib.att_pdu as att_pdu
import lib.gatt as gatt
from lib.uuid import UUID

class ClientError(Exception):
    def __init__(self, arg):
        self.msg = arg

def __handle_to_bytearray(handle):
	assert handle <= 0xFFFF
	assert handle >= 0
	return bytearray([handle & 0xFF, (handle >> 8) & 0xFF])

class _Service:
	def __init__(self, client, uuid, handleNo, endGroup):
		assert type(uuid) is UUID
		assert type(client) is GattClient
		assert type(handleNo) is int
		assert type(endGroup) is int

		self.client = client
		self.uuid = uuid

		self._handleNo = handleNo
		self._endGroup = endGroup 

		self.characteristics = []

	def discoverCharacteristics(self, uuid=[]):
		pass

	def __len__(self):
		return len(characteristics)

class _Characteristic:
	def __init__(self, client, service, uuid, handleNo, properties, 
		valHandleNo, endGroup):
		assert type(uuid) is UUID
		assert type(client) is GattClient
		assert type(service) is _Service
		assert type(handleNo) is int
		assert type(valHandleNo) is int
		assert type(endGroup) is int
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

	def discoverDescriptors(self):
		pass

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

class _Descriptor:
	def __init__(self, client, characteristic, uuid, handleNo):
		assert type(uuid) is UUID
		assert type(client) is GattClient
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

class GattClient:
	def __init__(self, socket, onDisconnect=None):
		self._socket = socket
		self._socket._setClient(self)

		self._disconnected = False
		self._onDisconnect = onDisconnect

		self._transactionLock = threading.Lock()
		self._responseSema = threading.BoundedSemaphore(1)
		self._responseSema.acquire() # decrement to 0
		
		self._current_request = None
		self._current_response = None

		self._valHandles = {}

		self.services = []

	def discoverServices(uuid=[]):
		pass

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
		assert self._current_request is None
		assert self._current_response is None
		
		try:
			if self._disconnected:
				return None

			self._current_request = req
			self._socket._send(req)
			self._responseSema.acquire()

			return self._current_response
		finally:
			self._current_request = None
			self._current_response = None
			self._transactionLock.release()

		return None

	def _handle_packet(self, pdu):
		op = pdu[0]
		if (att.isResponse(op) or (op == att.OP_ERROR 
			and self._current_request and self._current_response is None 
			and pdu[1] == self._current_request[0])):

			self._current_response = pdu
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



