from celery import task

import socket

from beetle.models import VirtualDevice, PrincipalGroup
from network.models import ConnectedDevice

from .application.manager import IPC_COMMAND_PATH

def _expand_principal_list(principals):
	ret = set()
	for principal in principals:
		if principal.name == "*":
			continue
		elif isinstance(principal, VirtualDevice):
			ret.add(principal)
		elif isinstance(principal, PrincipalGroup):
			for member in principal.members.all():
				ret.add(principal)
	return ret

def _get_device_instances(devices):
	ret = set()
	for device in devices:
		try:
			other_instance = ConnectedDevice.objects.get(device=device)
			ret.add(other_instance)
		except ConnectedDevice.DoesNotExist:
			pass
	return ret

def _make_mapping(from_device, to_device):
	msg = "map %d %d" % (from_device.id, to_device.id)
	s = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
	s.connect(IPC_COMMAND_PATH)
	s.send(msg)
	s.shutdown(socket.SHUT_RDWR)
	s.close()

@task
def add_new_device(device_instance_id):
	"""Create connections"""

	device_instance = ConnectedDevice.objects.get(id=device_instance_id)

	serve_to = _expand_principal_list(device_instance.device.serve_to.all())
	serve_to = _get_device_instances(serve_to)

	for other_device in serve_to:
		_make_mapping(device_instance, other_device)

	client_to = _expand_principal_list(device_instance.device.client_to.all())
	client_to = _get_device_instances(client_to)

	for other_device in client_to:
		_make_mapping(other_device, device_instance)


