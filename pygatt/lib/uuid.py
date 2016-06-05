
class UUID(object):
	def __init__(self, value, reverse=False):
		"""A Bluetooth UUID type

		Note: the value is stored internally in the order that the uuid is 
		printed.

		Args:
			value (int, str, bytearray) : the uuid
			reverse : whether to the input value is reversed 
		"""
		if isinstance(value, int):
			assert (value >> 16) == 0
			assert reverse is False
			self._raw = bytearray([(value >> 8) & 0xFF, value & 0xFF])
		
		elif isinstance(value, str):
			value = value.replace("-", "")
			assert reverse is False
			assert len(value) == 4 or len(value) == 32
			self._raw = bytearray.fromhex(value)

		elif isinstance(value, bytearray):
			assert len(value) == 2 or len(value) == 16
			if reverse:
				self._raw = value[::-1]
			else:
				self._raw = value[:]
		
		else:
			raise Exception("unsupported type")

	def raw(self):
		"""Return the uuid as a bytearray"""
		return self._raw[:]

	def __len__(self):
		return len(self._raw)

	def __str__(self):
		return ''.join('{:02X}'.format(x) for x in self._raw)

	def __eq__(self, other):
		return isinstance(other, self.__class__) and self._raw == other._raw

	def __ne__(self, other):
		return not self.__eq__(other)
		