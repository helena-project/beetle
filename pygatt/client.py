import threading

import lib.att as att
import lib.att_pdu as att_pdu
import lib.gatt as gatt
from lib.uuid import UUID

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
	def __init__(self, client, service, uuid, handleNo, properties, valHandleNo, 
		endGroup):
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
	 	self.permissions
		
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
		self._endGroup = endGroup

		self._subscribeCallback = None

		self._cccd = None
		self.descriptors = []

	def discoverDescriptors(self):
		pass

	def subscribe(self, callback):
		self._subscribeCallback = callback

	def unsubscribe(self):
		self._subscribeCallback = None

	def read(self, value):
		pass
	def write(self, value):
		pass

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
		pass
	def write(self, value):
		pass

class GattClient:
	def __init__(self, socket, onDisconnect=None):
		self._socket = socket
		self._socket._setClient(self)

		self._onDisconnect = onDisconnect

		self._transactSema = threading.BoundedSemaphore(1) 

		self._valHandles = {}

	def _add_val_handle(self, characteristic, valHandleNo):
		self._valHandles[valHandleNo] = characteristic

	def _handle_disconnect(self, err=None):
		if self._onDisconnect is not None:
			self._onDisconnect(err)

	def _transaction(self, pdu):
		self._transactSema.acquire()

	def _handle_packet(self):
		op = pdu[0]
		
		return None



	def discoverServices(uuid=[]):
		pass



