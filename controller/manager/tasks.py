
import socket
import warnings

from celery import task

from beetle.models import VirtualDevice, PrincipalGroup
from network.models import ConnectedDevice, DeviceMapping

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
			other_instance = ConnectedDevice.objects.get(
				device__name=device.name)
			ret.add(other_instance)
		except ConnectedDevice.DoesNotExist:
			pass
	return ret

def _filter_not_mapped_already_from(from_device, to_devices):
	existing_mappings = DeviceMapping.objects.filter(from_device=from_device, 
		to_device__in=to_devices)
	for mapping in existing_mappings:
		to_devices.discard(mapping.to_device)

def _filter_not_mapped_already_to(from_devices, to_device):
	existing_mappings = DeviceMapping.objects.filter(
		from_device__in=from_devices, to_device=to_device)
	for mapping in existing_mappings:
		from_devices.discard(mapping.from_device)

def _make_mapping(s, from_device, to_device):
	msg = "map %d %d" % (from_device.id, to_device.id)
	print msg
	s.send(msg)

def _make_mappings(device_instance, serve_to, client_to):
	if not serve_to and not client_to:
		return 

	s = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
	s.connect(IPC_COMMAND_PATH)

	for other_device in serve_to:
		_make_mapping(s, device_instance, other_device)

	for other_device in client_to:
		_make_mapping(s, other_device, device_instance)
	
	s.shutdown(socket.SHUT_RDWR)
	s.close()

@task
def connect_device_evt(device_instance_id):
	"""Create mappings when a device first connects."""

	device_instance = ConnectedDevice.objects.get(id=device_instance_id)

	serve_to = _expand_principal_list(device_instance.device.serve_to.all())
	serve_to = _get_device_instances(serve_to)

	client_to = _expand_principal_list(device_instance.device.client_to.all())
	client_to = _get_device_instances(client_to)

	_make_mappings(device_instance, serve_to, client_to)

@task
def update_device_evt(device_instance_id):
	"""Create mappings when a device has been updated."""

	device_instance = ConnectedDevice.objects.get(id=device_instance_id)

	serve_to = _expand_principal_list(device_instance.device.serve_to.all())
	serve_to = _get_device_instances(serve_to)
	_filter_not_mapped_already_from(device_instance, serve_to)

	client_to = _expand_principal_list(device_instance.device.client_to.all())
	client_to = _get_device_instances(client_to)
	_filter_not_mapped_already_to(client_to, device_instance)

	_make_mappings(device_instance, serve_to, client_to)

