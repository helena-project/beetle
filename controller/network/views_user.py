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
from gatt.uuid import convert_uuid, check_uuid
from access.lookup import query_can_map_static

# Create your views here.

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
			can_map, _ = query_can_map_static(
				x.device_instance.gateway_instance.gateway, 
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
