
import socket
from datetime import timedelta

from celery.decorators import task, periodic_task
from celery.utils.log import get_task_logger
from django.utils import timezone

from beetle.models import VirtualDevice, PrincipalGroup
from network.models import ConnectedGateway, ConnectedDevice, DeviceMapping, \
	ServiceInstance
from state.models import AdminAuthInstance, UserAuthInstance, \
	PasscodeAuthInstance, ExclusiveLease

from main.constants import GATEWAY_CONNECTION_TIMEOUT

from .application.manager import IPC_COMMAND_PATH

logger = get_task_logger(__name__)

def _expand_principal_list(principals):
	contains_all = False
	ret = set()
	for principal in principals:
		if principal.name == "*":
			contains_all = True
		elif isinstance(principal, VirtualDevice):
			ret.add(principal)
		elif isinstance(principal, PrincipalGroup):
			for member in principal.members.all():
				ret.add(principal)
	return ret, contains_all

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
	existing_mappings = DeviceMapping.objects.filter(
		from_device=from_device, to_device__in=to_devices)
	for mapping in existing_mappings:
		to_devices.discard(mapping.to_device)

def _filter_not_mapped_already_to(from_devices, to_device):
	existing_mappings = DeviceMapping.objects.filter(
		from_device__in=from_devices, to_device=to_device)
	for mapping in existing_mappings:
		from_devices.discard(mapping.from_device)

def _make_mappings(device_instance, serve_to=None, client_to=None):
	if not serve_to and not client_to:
		return 

	def _make_single_mapping(s, from_device, to_device):
		msg = "map %d %d" % (from_device.id, to_device.id)
		logger.info(msg)
		s.send(msg)

	s = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
	s.connect(IPC_COMMAND_PATH)

	if serve_to:
		for other_device in serve_to:
			_make_single_mapping(s, device_instance, other_device)

	if client_to:
		for other_device in client_to:
			_make_single_mapping(s, other_device, device_instance)
	
	s.shutdown(socket.SHUT_RDWR)
	s.close()

def _get_discoverers(device_instance):

	disc_by, disc_by_all = _expand_principal_list(
		device_instance.device.discoverable_by.all())

	def _discovery_allowed(other_instance):
		return disc_by_all or other_instance.device in disc_by

	services = ServiceInstance.objects.filter(device_instance=device_instance
		).values("service")

	potential_clients = ConnectedDevice.objects.filter(
		interested_services__in=services)
	potential_clients = set(x for x in potential_clients if _discovery_allowed(x))
	_filter_not_mapped_already_to(potential_clients, device_instance)

	return potential_clients

@task(name="connect_device")
def connect_device_evt(device_instance_id):
	"""Create mappings when a device first connects."""

	device_instance = ConnectedDevice.objects.get(id=device_instance_id)

	serve_to, _ = _expand_principal_list(device_instance.device.serve_to.all())
	serve_to = _get_device_instances(serve_to)

	client_to, _ = _expand_principal_list(device_instance.device.client_to.all())
	client_to = _get_device_instances(client_to)

	_make_mappings(device_instance, serve_to=serve_to, client_to=client_to)

@task(name="update_device")
def update_device_evt(device_instance_id):
	"""Create mappings when a device has been updated."""

	device_instance = ConnectedDevice.objects.get(id=device_instance_id)

	serve_to, _= _expand_principal_list(device_instance.device.serve_to.all())
	serve_to = _get_device_instances(serve_to)
	serve_to |= _get_discoverers(device_instance)
	_filter_not_mapped_already_from(device_instance, serve_to)

	client_to, _ = _expand_principal_list(device_instance.device.client_to.all())
	client_to = _get_device_instances(client_to)
	_filter_not_mapped_already_to(client_to, device_instance)

	_make_mappings(device_instance, serve_to=serve_to, client_to=client_to)

@task(name="register_device_interest")
def register_interest_service_evt(device_instance_id, service_uuid):
	"""Find devices in the network with the service."""

	device_instance = ConnectedDevice.objects.get(id=device_instance_id)

	def _discovery_allowed(other_instance):
		for principal in other_instance.device.discoverable_by.all():
			if principal.name == "*":
				return True
			elif principal.name == device_instance.device.name: 
				return True
			elif isinstance(principal, PrincipalGroup):
				for member in principal.members.all():
					if member.name == device_instance.device.name:
						return True
		return False

	potential_servers = set(x.device_instance for x in \
		ServiceInstance.objects.filter(service__uuid=service_uuid))
	potential_servers = set(x for x in potential_servers if _discovery_allowed(x))
	_filter_not_mapped_already_to(potential_servers, device_instance)

	_make_mappings(device_instance, client_to=potential_servers)

@periodic_task(
	run_every=timedelta(seconds=60), 
	name="timeout_gateways", 
	ignore_result=True
)
def timeout_gateways():
	"""Remove lingering gateway sessions"""

	logger.info("Timing out gateway instances.")

	threshold = timezone.now() - timedelta(seconds=GATEWAY_CONNECTION_TIMEOUT)
	ConnectedGateway.objects.filter(is_connected=False, 
		last_updated__lt=threshold).delete()

@periodic_task(
	run_every=timedelta(seconds=60), 
	name="timeout_access_control_state", 
	ignore_result=True
)
def timeout_access_control_state():
	"""Obliterate any expired state."""

	logger.info("Timing out access control state.")

	current_time = timezone.now()

	AdminAuthInstance.objects.filter(expire__lt=current_time).delete()
	UserAuthInstance.objects.filter(expire__lt=current_time).delete()
	PasscodeAuthInstance.objects.filter(expire__lt=current_time).delete()
	ExclusiveLease.objects.filter(expire__lt=current_time).delete()
