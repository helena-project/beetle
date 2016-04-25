from django.shortcuts import render
from django.http import JsonResponse, HttpResponse
from django.db.models import Q
from django.core import serializers
from django.utils import timezone
from django.views.decorators.gzip import gzip_page
from django.views.decorators.http import require_GET

from .models import Rule, RuleException, DynamicAuth, AdminAuth, SubjectAuth, PasscodeAuth, NetworkAuth

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

def _get_gateway_helper(gateway):
	"""
	Same as above without an entity.
	"""
	gateway = Gateway.objects.get(name=gateway)
	conn_gateway = ConnectedGateway.objects.get(gateway=gateway)
	return gateway, conn_gateway


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

	applicable_exceptions = RuleException.objects.filter(
		Q(from_entity=from_entity) | Q(from_entity__name="*"),
		Q(from_gateway=from_gateway) | Q(from_gateway__name="*"),
		Q(to_entity=to_entity) | Q(to_entity__name="*"),
		Q(to_gateway=to_gateway) | Q(to_gateway__name="*"))

	excluded_rule_ids = set()
	for rule in applicable_rules:
		rule_exceptions = applicable_exceptions.filter(
			rule=rule,
			service__name="*", 
			characteristic__name="*")
		if rule_exceptions.exists():
			excluded_rule_ids.add(rule.id)

	for exclude_id in excluded_rule_ids:
		applicable_rules = applicable_rules.exclude(id=exclude_id)
	
	response = {}
	if not applicable_rules.exists():
		response["result"] = False
		return JsonResponse(response)

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
	#				"excl" : False,
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
		service_rule_exceptions = applicable_exceptions.filter(
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

				char_rule_exceptions = service_rule_exceptions.filter(
					Q(characteristic=char_instance.char) | Q(characteristic__name="*"),
					rule=char_rule)
				if char_rule_exceptions.exists():
					continue

				#####################################
				# All checks pass, access permitted #
				#####################################
				if service.uuid not in services:
					services[service.uuid] = {}
				if char.uuid not in services[service.uuid]:
					services[service.uuid][char.uuid] = []
				if char_rule.id not in rules:
					dynamic_auth = []
					for auth in DynamicAuth.objects.filter(rule=char_rule):
						auth_obj = {
							"when" : auth.require_when,
						}
						if isinstance(auth, NetworkAuth):
							auth_obj["type"] = "network"
							auth_obj["ip"] = auth.ip_address
							auth_obj["priv"] = auth.is_private
						elif isinstance(auth, AdminAuth):
							auth_obj["type"] = "admin"
						elif isinstance(auth, SubjectAuth):
							auth_obj["type"] = "subject"
						elif isinstance(auth, PasscodeAuth):
							auth_obj["type"] = "passcode"
						dynamic_auth.append(auth_obj)

					# Put the rule in the result
					rules[char_rule.id] = {
						"prop" : char_rule.properties,
						"excl" : char_rule.exclusive,
						"int" : char_rule.integrity,
						"enc" : char_rule.encryption,
						"lease" : (timestamp + char_rule.lease_duration).strftime("%s"),
						"dauth" : dynamic_auth,
					}


				services[service.uuid][char.uuid].append(char_rule.id)
				
	if not rules:
		response["result"] = False
	else:
		response["result"] = True
		response["access"] = {
			"rules" : rules,
			"services" : services,
		}
	return JsonResponse(response)

@gzip_page
@require_GET
def view_rule_exceptions(request, rule_id):
	response = []
	for exception in RuleException.objects.filter(rule__id=int(rule_id)):
		response.append({
			"from_entity" : exception.from_entity.name,
			"from_gateway" : exception.from_gateway.name,
			"to_entity" : exception.to_entity.name,
			"to_gateway" : exception.to_gateway.name,
			"service" : exception.service.name,
			"characteristic" : exception.characteristic.name,
		})
	return JsonResponse(response, safe=False)