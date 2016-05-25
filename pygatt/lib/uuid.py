import uuid as pyuuid

class UUID:
	def __init__(self, value):
		if type(value) is int:
			assert (value >> 16) == 0
			self._raw = bytearray([value & 0xFF, (value >> 8) & 0xFF])
		elif type(value) is str:
			value = value.replace("-", "")
			assert len(value) == 4 or len(value) == 32
			self._raw = bytearray.fromhex(value)
		elif type(value) is bytearray:
			assert len(value) == 2 or len(value) == 16
			self._raw = value[:]
		else:
			raise Exception("unsupported type")

	def raw(self):
		return self._raw

	def __len__(self):
		return len(self._raw)

	def __str__(self):
		if self.__len__() == 2:
			return ''.join('{:02x}'.format(x) for x in self._raw.reverse())
		else:
			return uuid.UUID(bytes_le=self._raw).hex

	def __eq__(self, other):
		return isinstance(other, self.__class__) and self._raw == other._raw

	def __ne__(self, other):
		return not self.__eq__(other)