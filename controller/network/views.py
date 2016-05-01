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

from .models import ConnectedGateway, ConnectedPrincipal, CharInstance, \
	ServiceInstance
from beetle.models import Gateway, Principal
from gatt.models import Service, Characteristic
from gatt.shared import convert_uuid, check_uuid


# Create your views here.

@transaction.atomic
@csrf_exempt
@require_http_methods(["POST", "DELETE"])
def connect_gateway(request, gateway):
	"""
	Connect or disconnect a gateway to Beetle network
	"""
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
			gateway_conn.last_seen = timezone.now()
			ConnectedPrincipal.objects.filter(gateway=gateway_conn).delete()

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

def _load_services_and_characteristics(services, principal_conn):
	"""
	Parse the services and characteristics
	"""
	# first clear any existing
	ServiceInstance.objects.filter(principal=principal_conn).delete()
	CharInstance.objects.filter(principal=principal_conn).delete()

	# parse and setup service/char heirarchy
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
			principal=principal_conn)
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
				char=char, 
				principal=principal_conn, 
				service=service_ins)
			char_ins.save()

@transaction.atomic
@csrf_exempt
@require_POST
def connect_principal(request, gateway, principal, remote_id):
	"""
	Connect an application or peripheral
	"""
	if gateway == "*" or principal == "*":
		return HttpResponse(status=400)
	remote_id = int(remote_id)

	gateway_conn = ConnectedGateway.objects.get(gateway__name=gateway)
	principal, created = Principal.objects.get_or_create(name=principal)
	if created:
		pass

	try:
		principal_conn = ConnectedPrincipal.objects.get(
			principal=principal, 
			gateway=gateway_conn)
		principal_conn.remote_id = remote_id
		principal_conn.last_seen = timezone.now()
	except ConnectedPrincipal.DoesNotExist:
		principal_conn = ConnectedPrincipal(
			principal=principal, 
			gateway=gateway_conn, 
			remote_id=remote_id)

	gateway_conn.last_seen = timezone.now()
	gateway_conn.save()
	principal_conn.save()

	services = json.loads(request.body)
	response = _load_services_and_characteristics(services, principal_conn)
	if response is None:
		return HttpResponse("connected")
	else:
		return response

@transaction.atomic
@csrf_exempt
@require_http_methods(["DELETE", "PUT"])
def update_principal(request, gateway, remote_id):
	"""
	Disconnect an application or peripheral
	"""
	if gateway == "*":
		return HttpResponse(status=400)
	remote_id = int(remote_id)

	gateway_conn = ConnectedGateway.objects.get(gateway__name=gateway)
	gateway_conn.last_seen = timezone.now()
	gateway_conn.save()

	if request.method == "DELETE":
		##############
		# Disconnect #
		##############
		principal_conns = ConnectedPrincipal.objects.filter(
			gateway=gateway_conn, 
			remote_id=remote_id)
		principal_conns.delete()

		return HttpResponse("disconnected")

	elif request.method == "PUT":
		##########
		# Update #
		##########
		principal_conn = ConnectedPrincipal.objects.get(
			gateway=gateway_conn, remote_id=remote_id)
		principal_conn.last_seen = timezone.now()
		principal_conn.save()

		services = json.loads(request.body)
		response = _load_services_and_characteristics(services, principal_conn)

		if response is None:
			return HttpResponse("updated")
		else:
			return response
	
	else:
		return HttpResponse(status=405)

@require_GET
@gzip_page
def view_principal(request, principal, detailed=True):
	"""
	Returns in json, the principal's service anc characteristic uuids
	"""
	principal_conns = ConnectedPrincipal.objects.filter(
		principal__name=principal).order_by("-last_seen")
	
	if len(principal_conns) == 0:
		return HttpResponse(status=202)
	
	response = []
	principal_conn = principal_conns[0]
	for service_ins in ServiceInstance.objects.filter(principal=principal_conn):
		if detailed:
			response.append({
				"name": service_ins.service.name,
				"uuid": service_ins.service.uuid,
				"chars": [{
					"name" : x.char.name,
					"uuid" : x.char.uuid
				} for x in CharInstance.objects.filter(service=service_ins)]
			})
		else:
			response.append({
				"uuid": service_ins.service.uuid,
				"chars": [x.char.uuid for x in 
					CharInstance.objects.filter(service=service_ins)]})
	
	return JsonResponse(response, safe=False)

@require_GET
@gzip_page
def discover_principals(request):
	"""
	Get list of connnected principals
	"""
	response = []
	for conn_principal in ConnectedPrincipal.objects.all():
		response.append({
			"principal" : {
				"name" : conn_principal.principal.name,
				"id" : conn_principal.remote_id,
			},
			"gateway" : {
				"name" : conn_principal.gateway.gateway.name,
				"ip" : conn_principal.gateway.ip_address,
				"port" : conn_principal.gateway.port,
			},
		});
	return JsonResponse(response, safe=False)

@require_GET
@gzip_page
def discover_with_uuid(request, uuid, is_service=True):
	"""
	Get devices with characteristic 
	"""
	if check_uuid(uuid) == False:
		return HttpResponse("invalid uuid %s" % uuid, status=400)
	uuid = convert_uuid(uuid)

	response = []
	if is_service:
		qs = ServiceInstance.objects.filter(service__uuid=uuid)
	else:
		qs = CharInstance.objects.filter(char__uuid=uuid)
	
	for x in qs:
		response.append({
			"principal" : {
				"name" : x.principal.principal.name,
				"id" : x.principal.remote_id,
			},
			"gateway" : {
				"name" : x.principal.gateway.gateway.name,
				"ip" : x.principal.gateway.ip_address,
				"port" : x.principal.gateway.port,
			},
		});

	return JsonResponse(response, safe=False)

@require_GET
def find_gateway(request, gateway):
	try:
		gateway_conn = ConnectedGateway.objects.get(gateway__name=gateway)
	except ConnectedGateway.DoesNotExist:
		return HttpResponse("could not locate " + gateway, status=400)
		
	return JsonResponse({
		"ip" : gateway_conn.ip_address,
		"port" : gateway_conn.port,
	})
