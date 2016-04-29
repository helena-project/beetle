from django.shortcuts import render, render_to_response
from django.http import JsonResponse, HttpResponse
from django.db.models import Q
from django.core import serializers
from django.utils import timezone
from django.views.decorators.gzip import gzip_page
from django.views.decorators.http import require_GET

from .models import Rule, RuleException, DynamicAuth, AdminAuth, UserAuth, \
	PasscodeAuth, NetworkAuth

from beetle.models import Principal, Gateway, Contact
from gatt.models import Service, Characteristic
from network.models import ConnectedGateway, ConnectedPrincipal, ServiceInstance, \
	CharInstance
from network.shared import get_gateway_helper, get_gateway_and_principal_helper

import dateutil.parser
import cronex
import json
import base64

def _rule_is_static_subset(rule1, rule2):
	"""
	Returns whether a rule1 is a static subset of rule2.
	"""
	def _lte(a, b):
		return a == b or b.name == "*"

	is_subset = True
	is_subset &= _lte(rule1.from_principal, rule2.from_principal)
	is_subset &= _lte(rule1.from_gateway, rule2.from_gateway)
	is_subset &= _lte(rule1.to_principal, rule2.to_principal)
	is_subset &= _lte(rule1.to_gateway, rule2.to_gateway)
	is_subset &= _lte(rule1.service, rule2.service)
	is_subset &= _lte(rule1.characteristic, rule2.characteristic)
	
	if not is_subset:
		return is_subset

	is_subset &= set(rule1.properties) == set(rule2.properties)
	is_subset &= rule1.exclusive == rule2.exclusive
	is_subset &= rule1.integrity == rule2.integrity
	is_subset &= rule1.encryption == rule2.encryption
	is_subset &= rule1.lease_duration == rule2.lease_duration

	return is_subset

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
	from_gateway, from_principal, conn_from_gateway, conn_from_principal = \
		get_gateway_and_principal_helper(from_gateway, from_id)

	to_id = int(to_id)
	to_gateway, to_principal, conn_to_gateway, conn_to_principal = \
		get_gateway_and_principal_helper(to_gateway, to_id)

	applicable_rules = Rule.objects.filter(
		Q(from_principal=from_principal) | Q(from_principal__name="*"),
		Q(from_gateway=from_gateway) | Q(from_gateway__name="*"),
		Q(to_principal=to_principal) | Q(to_principal__name="*"),
		Q(to_gateway=to_gateway) | Q(to_gateway__name="*"),
		active=True)

	if timestamp is not None:
		applicable_rules = applicable_rules.filter(
			start__lte=timestamp, expire__gte=timestamp) \
			| applicable_rules.filter(expire__isnull=True)

	applicable_exceptions = RuleException.objects.filter(
		Q(from_principal=from_principal) | Q(from_principal__name="*"),
		Q(from_gateway=from_gateway) | Q(from_gateway__name="*"),
		Q(to_principal=to_principal) | Q(to_principal__name="*"),
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
	# 				"lease" : 1000,		# Expiration time
	#				"excl" : False,
	#				"dauth" : [
	#					... 			# Additional auth	
	#				]
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
	for service_instance in ServiceInstance.objects.filter(principal=conn_from_principal):
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
					unsatisfiable = False

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
							if auth.admin.id == Contact.NULL:
								# Rule is unsatisfiable: there is no admin
								unsatisfiable = True
								break
						elif isinstance(auth, UserAuth):
							auth_obj["type"] = "user"
							if to_principal.owner.id == Contact.NULL:
								# Rule is unsatisfiable: there is user to authenticate
								unsatisfiable = True
								break
						elif isinstance(auth, PasscodeAuth):
							auth_obj["type"] = "passcode"
						dynamic_auth.append(auth_obj)
					
					if unsatisfiable:
						continue

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
def view_rule_exceptions(request, rule):
	"""
	View in admin UI.
	"""
	response = []
	for exception in RuleException.objects.filter(rule__name=rule):
		response.append({
			"from_principal" : exception.from_principal.name,
			"from_gateway" : exception.from_gateway.name,
			"to_principal" : exception.to_principal.name,
			"to_gateway" : exception.to_gateway.name,
			"service" : exception.service.name,
			"characteristic" : exception.characteristic.name,
		})
	return JsonResponse(response, safe=False)

