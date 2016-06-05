
def gateway(arg):
	"""Return a regex that captures gateway name"""
	assert isinstance(arg, str)
	return r"(?P<%s>[\w_\-@\' \.]+)" % (arg,)

def device(arg):
	"""Return a regex that captures device name"""
	assert isinstance(arg, str)
	return r"(?P<%s>[\w_\-@\' \.]+)" % (arg,)
	