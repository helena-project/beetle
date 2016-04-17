from django.shortcuts import render
from django.db import transaction
from django.http import JsonResponse, HttpResponse

from ipware.ip import get_ip

import json
from datetime import datetime

from .models import ConnectedGateway, ConnectedEntity, CharInstance, ServiceInstance
from beetle.models import Gateway, Entity
from gatt.models import Service, Characteristic
from gatt.shared import convert_uuid, check_uuid


# Create your views here.

@transaction.atomic
def connect_gateway(request, gateway):
	"""
	Connect a gateway to Beetle network
	"""
	if gateway == "*":
		return HttpResponse(status=400)

	gateway = Gateway.objects.get(name=gateway)
	gateway_conn, created = ConnectedGateway.objects.get_or_create(gateway=gateway)
	if not created:
		gateway_conn.last_seen = datetime.now()
	ip_address = get_ip(request)
	if ip_address is None:
		return HttpResponse(status=400)
	gateway_conn.ip_address = ip_address
	gateway_conn.save()

	return HttpResponse("connected")

@transaction.atomic
def disconnect_gateway(request, gateway):
	"""
	Disconnect a gateway to Beetle network
	"""
	if gateway == "*":
		return HttpResponse(status=400)

	gateway = Gateway.objects.get(gateway)
	gateway_conn = ConnectedGateway.get(gateway=gateway)
	gateway_conn.delete()

	return HttpResponse("disconnected")

def _load_services_and_characteristics(services, entity_conn):
	"""
	Parse the services and characteristics
	"""
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
			entity=entity_conn)
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
				entity=entity_conn, 
				service=service_ins)
			char_ins.save()

@transaction.atomic
def connect_entity(request, gateway, entity, remote_id):
	"""
	Connect an application or peripheral
	"""
	if gateway == "*" or entity == "*":
		return HttpResponse(status=400)
	remote_id = int(remote_id)

	gateway_conn = ConnectedGateway.objects.get(gateway__name=gateway)
	entity, created = Entity.objects.get_or_create(name=entity)
	if created:
		pass

	try:
		entity_conn = ConnectedEntity.objects.get(entity=entity, gateway=gateway_conn)
		entity_conn.remote_id = remote_id
		entity_conn.last_seen = datetime.now()
	except ConnectedEntity.DoesNotExist:
		entity_conn = ConnectedEntity(
			entity=entity, 
			gateway=gateway_conn, 
			remote_id=remote_id)

	gateway_conn.last_seen = datetime.now()
	gateway_conn.save()
	entity_conn.save()

	services = json.loads(request.body)
	response = _load_services_and_characteristics(services, entity_conn)
	if response is None:
		return HttpResponse("connected")
	else:
		return response

@transaction.atomic
def disconnect_entity(request, gateway, remote_id):
	"""
	Disconnect an application or peripheral
	"""
	if gateway == "*":
		return HttpResponse(status=400)
	remote_id = int(remote_id)

	entity_conns = ConnectedEntity.objects.filter(
		gateway__gateway__name=gateway, 
		remote_id__lte=remote_id)
	entity_conns.delete()

	return HttpResponse("disconnected")

def view_entity(request, entity, detailed=True):
	"""
	Returns in json, the entity's service anc characteristic uuids
	"""
	entity_conns = ConnectedEntity.objects.filter(entity__name=entity).order_by("-last_seen")
	
	if len(entity_conns) == 0:
		return HttpResponse(status=202)
	
	response = []
	entity_conn = entity_conns[0]
	for service_ins in ServiceInstance.objects.filter(entity=entity_conn):
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
				"chars": [x.char.uuid for x in CharInstance.objects.filter(service=service_ins)]})
	
	return JsonResponse(response, safe=False)

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
			"entity" : {
				"name" : x.entity.entity.name,
				"id" : x.entity.remote_id,
			},
			"gateway" : {
				"name" : x.entity.gateway.gateway.name,
				"ip" : x.entity.gateway.ip_address,
				"port" : x.entity.gateway.port,
			},
		});

	return JsonResponse(response, safe=False)