import re

UUID_RE = re.compile(r"^(\*|[a-f0-9A-F]{4}|([a-f0-9A-F]{8}-([a-f0-9A-F]{4}-){3}[a-f0-9A-F]{12}))$")

BLUETOOTH_BASE_UUID = "0000-1000-8000-00805F9B34FB"

def convert_uuid(uuid):
	"""Convert uuid to 4 bytes if short."""

	uuid = uuid.upper()
	if len(uuid) > 4 and uuid[9:] == BLUETOOTH_BASE_UUID:
		uuid = uuid[6:8] + uuid[4:6]
		
	return uuid

def check_uuid(uuid):
	"""Verify that the string is a valid uuid"""

	uuid = uuid.upper()
	return UUID_RE.match(uuid) is not None