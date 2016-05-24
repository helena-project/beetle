
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
		return ""