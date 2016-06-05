
import json

from django.db import transaction
from django.http import HttpResponse
from django.views.decorators.http import require_http_methods, \
	require_POST
from django.views.decorators.csrf import csrf_exempt

from ipware.ip import get_ip

from .lookup import get_gateway_and_device_helper
from .models import ConnectedGateway, ConnectedDevice, CharInstance, \
	ServiceInstance, DeviceMapping

from beetle.models import BeetleGateway, VirtualDevice
from gatt.models import Service, Characteristic
from gatt.uuid import convert_uuid, check_uuid

@transaction.atomic
@csrf_exempt
@require_http_methods(["POST", "DELETE"])
def connect_gateway(request, gateway):
	"""Connect or disconnect a gateway to Beetle network"""

	if gateway == "*":
		return HttpResponse(status=400)

	if request.method == "POST":
		##############
		# Connection #
		##############
		port = int(request.POST["port"])
		ip_address = get_ip(request)
		if ip_address is None:
			return HttpResponse(status=400)

		try:
			gateway = BeetleGateway.objects.get(name=gateway)
		except BeetleGateway.DoesNotExist:
			return HttpResponse("no gateway named " + gateway, status=400)

		gateway_conn, created = ConnectedGateway.objects.get_or_create(
			gateway=gateway)
		if not created:
			ConnectedDevice.objects.filter(gateway_instance=gateway_conn).delete()

		gateway_conn.ip_address = ip_address
		gateway_conn.port = port
		gateway_conn.save()

		return HttpResponse("connected")

	elif request.method == "DELETE":
		#################
		# Disconnection #
		#################
		gateway_conn = ConnectedGateway.objects.get(gateway__name=gateway)
		gateway_conn.delete()

		return HttpResponse("disconnected")
		
	else:
		return HttpResponse(status=405)

def __load_services_and_characteristics(services, device_conn):
	"""Parse the services and characteristics"""

	# first clear any existing
	ServiceInstance.objects.filter(device_instance=device_conn).delete()
	CharInstance.objects.filter(device_instance=device_conn).delete()

	# parse and setup service/char hierarchy
	for service_obj in services:
		service_uuid = service_obj["uuid"]
		if not check_uuid(service_uuid):
			return HttpResponse("invalid uuid %s" % service_uuid, status=400)
		else:
			service_uuid = convert_uuid(service_uuid)
		service, created = Service.objects.get_or_create(uuid=service_uuid)
		if created:
			pass

		service_ins, _ = ServiceInstance.objects.get_or_create(
			service=service, 
			device_instance=device_conn)
		service_ins.save()

		for char_uuid in service_obj["chars"]:
			if not check_uuid(char_uuid):
				return HttpResponse("invalid uuid %s" % char_uuid, status=400)
			else:
				char_uuid = convert_uuid(char_uuid)
			char, created = Characteristic.objects.get_or_create(uuid=char_uuid)
			if created:
				pass

			char_ins, _ = CharInstance.objects.get_or_create(
				characteristic=char, 
				device_instance=device_conn, 
				service_instance=service_ins)
			char_ins.save()

@transaction.atomic
@csrf_exempt
@require_POST
def connect_device(request, gateway, device, remote_id):
	"""Connect an application or peripheral"""

	if gateway == "*" or device == "*":
		return HttpResponse(status=400)
	remote_id = int(remote_id)

	gateway_conn = ConnectedGateway.objects.get(gateway__name=gateway)
	device, created = VirtualDevice.objects.get_or_create(name=device)
	if created:
		pass

	try:
		device_conn = ConnectedDevice.objects.get(
			device=device, 
			gateway_instance=gateway_conn)
		device_conn.remote_id = remote_id
	except ConnectedDevice.DoesNotExist:
		device_conn = ConnectedDevice(
			device=device, 
			gateway_instance=gateway_conn, 
			remote_id=remote_id)

	gateway_conn.save()
	device_conn.save()

	services = json.loads(request.body)
	response = __load_services_and_characteristics(services, device_conn)
	if response is None:
		return HttpResponse("connected")
	else:
		return response

@transaction.atomic
@csrf_exempt
@require_http_methods(["DELETE", "PUT"])
def update_device(request, gateway, remote_id):
	"""Disconnect an application or peripheral"""

	if gateway == "*":
		return HttpResponse(status=400)
	remote_id = int(remote_id)

	gateway_conn = ConnectedGateway.objects.get(gateway__name=gateway)
	gateway_conn.save()

	if request.method == "PUT":
		##########
		# Update #
		##########
		device_conn = ConnectedDevice.objects.get(
			gateway_instance=gateway_conn, remote_id=remote_id)
		device_conn.save()

		services = json.loads(request.body)
		response = __load_services_and_characteristics(services, device_conn)

		if response is None:
			return HttpResponse("updated")
		else:
			return response

	elif request.method == "DELETE":		
		##############
		# Disconnect #
		##############
		device_conns = ConnectedDevice.objects.filter(
			gateway_instance=gateway_conn, 
			remote_id=remote_id)
		device_conns.delete()

		return HttpResponse("disconnected")
	
	else:
		return HttpResponse(status=405)

@transaction.atomic
@csrf_exempt
@require_http_methods(["POST", "DELETE"])
def map_devices(request, from_gateway, from_id, to_gateway, to_id):
	"""Insert or remove mapping between devices"""

	from_id = int(from_id)
	_, _, _, conn_from_device = \
		get_gateway_and_device_helper(from_gateway, from_id)

	to_id = int(to_id)
	_, _, _, conn_to_device = \
		get_gateway_and_device_helper(to_gateway, to_id)

	if request.method == "POST":
		mapping, _ = DeviceMapping.objects.get_or_create(
			from_device=conn_from_device, 
			to_device=conn_to_device)
		mapping.save()
		return HttpResponse()

	elif request.method == "DELETE":
		mappings = DeviceMapping.objects.filter(
			from_device=conn_from_device, 
			to_device=conn_to_device)
		mappings.delete()
		return HttpResponse()

	return HttpResponse(status=405)

@transaction.atomic
@csrf_exempt
@require_POST
def register_interest(request, gateway, remote_id, uuid, is_service=True):
	"""Register interest for a service or characteristic"""

	device_instance = ConnectedDevice.objects.get(
		gateway_instance__gateway__name=gateway, remote_id=remote_id)

	if not check_uuid(uuid):
		return HttpResponse(status=400)

	uuid = convert_uuid(uuid) 

	if is_service:
		try:
			service = Service.objects.get(uuid=uuid)
			device_instance.interested_services.add(service)
		except Service.DoesNotExist:
			pass
	else:
		try:
			characteristic = Characteristic.objects.get(uuid=uuid)
			device_instance.interested_characteristics.add(characteristic)
		except Characteristic.DoesNotExist:
			pass

	device_instance.save()

	return HttpResponse()
	