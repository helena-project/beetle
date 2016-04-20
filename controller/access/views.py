from django.shortcuts import render
from django.http import JsonResponse, HttpResponse
from django.db.models import Q
from django.core import serializers
from django.utils import timezone
from django.views.decorators.gzip import gzip_page
from django.views.decorators.http import require_GET

from .models import Rule

from beetle.models import Entity, Gateway
from gatt.models import Service, Characteristic
from network.models import ConnectedGateway, ConnectedEntity, ServiceInstance, CharInstance

import dateutil.parser
import cronex

# Create your views here.

def _get_gateway_and_entity_helper(gateway, remote_id):
	"""
	Gateway is a string, remote_id is an int.
	"""
	gateway = Gateway.objects.get(name=gateway)
	conn_gateway = ConnectedGateway.objects.get(gateway=gateway)
	conn_entity = ConnectedEntity.objects.get(gateway=conn_gateway, 
		remote_id=remote_id)
	entity = conn_entity.entity
	return gateway, entity, conn_gateway, conn_entity

@gzip_page
@require_GET
def query_can_map(request, from_gateway, from_id, to_gateway, to_id, timestamp=None):
	"""
	Return whether fromId at fromGateway can connect to toId at toGateway
	"""

	if timestamp is None and "timestamp" in request.GET:
		timestamp = dateutil.parser.parse(request.GET["timestamp"])
	else:
		timestamp = timezone.now()

	from_id = int(from_id)
	from_gateway, from_entity, conn_from_gateway, conn_from_entity = \
		_get_gateway_and_entity_helper(from_gateway, from_id)

	to_id = int(to_id)
	to_gateway, to_entity, conn_to_gateway, conn_to_entity = \
		_get_gateway_and_entity_helper(to_gateway, to_id)

	applicable_rules = Rule.objects.filter(
		Q(from_entity=from_entity) | Q(from_entity__name="*"),
		Q(from_gateway=from_gateway) | Q(from_gateway__name="*"),
		Q(to_entity=to_entity) | Q(to_entity__name="*"),
		Q(to_gateway=to_gateway) | Q(to_gateway__name="*"),
		active=True)

	if timestamp is not None:
		applicable_rules = applicable_rules.filter(
			start__lte=timestamp, expire__gte=timestamp) \
			| applicable_rules.filter(expire__isnull=True)
	
	response = {}
	if not applicable_rules.exists():
		response["result"] = False
		return JsonResponse(response)
	else:
		response["result"] = True

	# Response format:
	# ================
	# {
	# 	"result" : True,
	# 	"access" : {
	# 		"rules" : {
	# 			1 : {					# Spec of the rule
	# 				"prop" : "rwni",
	# 				"int" : True,
	# 				"enc" : False,
	# 				"lease" : 1000,
	# 			}, 
	# 		},
	# 		"services": {
	# 			"2A00" : {				# Service
	# 				"2A01" : [1]		# Char to applicable rules
	#			},
	# 		},
	# 	}
	# }

	services = {}
	rules = {}
	for service_instance in ServiceInstance.objects.filter(entity=conn_from_entity):
		service_rules = applicable_rules.filter(
			Q(service=service_instance.service) | Q(service__name="*"))
		service = service_instance.service

		for char_instance in CharInstance.objects.filter(service=service_instance):
			char = char_instance.char

			char_prop = set()
			char_int = False
			char_enc = False
			char_lease = timestamp

			char_rules = service_rules.filter(
				Q(characteristic=char_instance.char) | Q(characteristic__name="*"))
			for char_rule in char_rules:
				if service.uuid not in services:
					services[service.uuid] = {}
				if char.uuid not in services[service.uuid]:
					services[service.uuid][char.uuid] = []
				if char_rule.id not in rules:
					# Put the rule in the result
					rules[char_rule.id] = {
						"prop" : char_rule.properties,
						"int" : char_rule.integrity,
						"enc" : char_rule.encryption,
						"lease" : (timestamp + char_rule.lease_duration).strftime("%s"),
					}
				services[service.uuid][char.uuid].append(char_rule.id)
				
	response["access"] = {
		"rules" : rules,
		"services" : services,
	}
	return JsonResponse(response)