
def device_id(arg):
	"""Return a regex that captures device id"""
	assert type(arg) is str
	return r"(?P<%s>\d+)" % (arg,)