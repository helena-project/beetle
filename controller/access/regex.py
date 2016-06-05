
def rule(arg):
	"""Return a regex that captures rule name"""
	assert isinstance(arg, str)
	return r"(?P<%s>\w+)" % (arg,)

def rule_id(arg):
	"""Return a regex that captures rule id"""
	assert isinstance(arg, str)
	return r"(?P<%s>\d+)" % (arg,)

def exclusive_id(arg):
	"""Return a regex that captures exclusive id"""
	assert isinstance(arg, str)
	return r"(?P<%s>\d+)" % (arg,)