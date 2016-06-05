
from gatt.uuid import UUID_REGEX

def uuid(arg):
	"""Returns a regex that matches and and captures a uuid""" 
	assert isinstance(arg, str)
	return (r"(?P<%s>" + UUID_REGEX + ")") % (arg,)
	