
def rule(arg):
	"""Return a regex that captures rule name"""
	assert type(arg) is str
	return r"(?P<%s>\w+)" % (arg,)

def rule_id(arg):
	"""Return a regex that captures rule id"""
	assert type(arg) is str
	return r"(?P<%s>\d+)" % (arg,)

def exclusive_id(arg):
	"""Return a regex that captures exclusive id"""
	assert type(arg) is str
	return r"(?P<%s>\d+)" % (arg,)