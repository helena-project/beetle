import lib.att as att
import lib.att_pdu as att_pdu
import lib.gatt as gatt
from lib.uuid import UUID

class _Service:
	def __init__(self, client, uuid, startGroup, endGroup):
		assert type(uuid) is UUID
		assert type(client) is GattClient
		assert type(startGroup) is int
		assert type(endGroup) is int

		self.client = client
		self.uuid = uuid

		self._startGroup = startGroup
		self._endGroup = endGroup 

class _Characteristic:
	pass
class _Descriptor:
	pass

class GattClient:
	def __init__(self, socket, onDisconnect=None):
		self._socket = socket
		self._socket._setClient(self)

		self._onDisconnect = onDisconnect

	def _handle_disconnect(self, err=None):
		if self._onDisconnect is not None:
			self._onDisconnect(err)

	def _handle_packet(self):
		op = pdu[0]
		
		return None

	def discoverServices(uuid=[]):
		pass



