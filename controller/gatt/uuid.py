import re

UUID_REGEX = "[a-f0-9A-F]{4}|([a-f0-9A-F]{8}-([a-f0-9A-F]{4}-){3}[a-f0-9A-F]{12})"

BLUETOOTH_BASE_UUID = "0000-1000-8000-00805F9B34FB"

def convert_uuid(uuid):
	"""Convert uuid to 2 bytes if short."""

	assert type(uuid) is str or type(uuid) is unicode

	uuid = uuid.upper()
	if len(uuid) > 4 and uuid[9:] == BLUETOOTH_BASE_UUID:
		uuid = uuid[4:8]
		
	return uuid

__UUID_RE = re.compile(r"^("  + UUID_REGEX + ")$")
def check_uuid(uuid):
	"""Verify that the string is a valid uuid"""

	assert type(uuid) is str or type(uuid) is unicode

	uuid = uuid.upper()
	return __UUID_RE.match(uuid) is not None