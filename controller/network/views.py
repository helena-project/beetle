from django.shortcuts import render
from django.db import transaction
from django.http import JsonResponse, HttpResponse
from django.utils import timezone
from django.views.decorators.http import require_http_methods, require_GET, \
	require_POST
from django.views.decorators.gzip import gzip_page
from django.views.decorators.csrf import csrf_exempt

from ipware.ip import get_ip

import json

from .models import ConnectedGateway, ConnectedDevice, CharInstance, \
	ServiceInstance

from beetle.models import Gateway, VirtualDevice
from state.models import ExclusiveLease
from gatt.models import Service, Characteristic
from gatt.shared import convert_uuid, check_uuid
from access.shared import query_can_map_static

# Create your views here.

# TODO replace
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
		port = int(request.POST["port"]);
		ip_address = get_ip(request)
		if ip_address is None:
			return HttpResponse(status=400)

		try:
			gateway = Gateway.objects.get(name=gateway)
		except Gateway.DoesNotExist:
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

def _load_services_and_characteristics(services, device_conn):
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
	response = _load_services_and_characteristics(services, device_conn)
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
		response = _load_services_and_characteristics(services, device_conn)

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

@require_GET
@gzip_page
def view_device(request, device, detailed=True):
	"""Returns in json, the device's service anc characteristic uuids"""

	device_conns = ConnectedDevice.objects.filter(device__name=device)
	
	if len(device_conns) == 0:
		return HttpResponse(status=202)
	
	response = []
	device_conn = device_conns[0]
	for service_ins in ServiceInstance.objects.filter(device_instance=device_conn):
		if detailed:
			response.append({
				"name": service_ins.service.name,
				"uuid": service_ins.service.uuid,
				"chars": [{
					"name" : x.characteristic.name,
					"uuid" : x.characteristic.uuid
				} for x in CharInstance.objects.filter(service_instance=service_ins)]
			})
		else:
			response.append({
				"uuid": service_ins.service.uuid,
				"chars": [x.characteristic.uuid for x in 
					CharInstance.objects.filter(service_instance=service_ins)]})
	
	return JsonResponse(response, safe=False)

@require_GET
@gzip_page
def discover_devices(request):
	"""Get list of connnected devices"""

	response = []
	for conn_device in ConnectedDevice.objects.all():
		response.append({
			"device" : {
				"name" : conn_device.device.name,
				"id" : conn_device.remote_id,
			},
			"gateway" : {
				"name" : conn_device.gateway_instance.gateway.name,
				"ip" : conn_device.gateway_instance.ip_address,
				"port" : conn_device.gateway_instance.port,
			},
		});
	return JsonResponse(response, safe=False)

@require_GET
@gzip_page
def discover_with_uuid(request, uuid, is_service=True):
	"""Get devices with characteristic"""

	if check_uuid(uuid) == False:
		return HttpResponse("invalid uuid %s" % uuid, status=400)
	uuid = convert_uuid(uuid)

	###
	gateway = request.GET.get("gateway", None)
	remote_id = request.GET.get("remote_id", None)
	gateway_conn = None
	device_conn = None
	if gateway is not None and remote_id is not None:
		gateway_conn = ConnectedGateway.objects.get(
			gateway__name=gateway)
		device_conn = ConnectedDevice.objects.get(
			gateway_instance=gateway_conn, remote_id=int(remote_id))
	###

	current_time = timezone.now()

	response = []
	if is_service:
		qs = ServiceInstance.objects.filter(service__uuid=uuid)
	else:
		qs = CharInstance.objects.filter(characteristic__uuid=uuid)
	
	for x in qs:

		if gateway_conn is not None and device_conn is not None:
			can_map, _ = query_can_map_static(x.device_instance.gateway_instance.gateway, 
				x.device_instance.device, gateway_conn.gateway, 
				device_conn.device, current_time)
		else:
			can_map = True
		
		if not can_map:
			continue

		response.append({
			"device" : {
				"name" : x.device_instance.device.name,
				"id" : x.device_instance.remote_id,
			},
			"gateway" : {
				"name" : x.device_instance.gateway_instance.gateway.name,
				"ip" : x.device_instance.gateway_instance.ip_address,
				"port" : x.device_instance.gateway_instance.port,
			},
		});

	return JsonResponse(response, safe=False)

@require_GET
def find_gateway(request, gateway):
	"""Get information on how to reach a gateway"""
	
	try:
		gateway_conn = ConnectedGateway.objects.get(gateway__name=gateway)
	except ConnectedGateway.DoesNotExist:
		return HttpResponse("could not locate " + gateway, status=400)
		
	return JsonResponse({
		"ip" : gateway_conn.ip_address,
		"port" : gateway_conn.port,
	})
