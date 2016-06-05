
def device_id(arg):
	"""Return a regex that captures device id"""
	assert isinstance(arg, str)
	return r"(?P<%s>\d+)" % (arg,)
	