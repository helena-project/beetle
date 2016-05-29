
class UUID:
	def __init__(self, value, reverse=False):
		if type(value) is int:
			assert (value >> 16) == 0
			assert reverse == False
			self._raw = bytearray([(value >> 8) & 0xFF, value & 0xFF])
		
		elif type(value) is str:
			value = value.replace("-", "")
			assert reverse == False
			assert len(value) == 4 or len(value) == 32
			self._raw = bytearray.fromhex(value)

		elif type(value) is bytearray:
			assert len(value) == 2 or len(value) == 16
			if reverse:
				self._raw = value[::-1]
			else:
				self._raw = value[:]
		
		else:
			raise Exception("unsupported type")

	def raw(self):
		return self._raw

	def __len__(self):
		return len(self._raw)

	def __str__(self):
		return ''.join('{:02x}'.format(x) for x in self._raw)

	def __eq__(self, other):
		return isinstance(other, self.__class__) and self._raw == other._raw

	def __ne__(self, other):
		return not self.__eq__(other)