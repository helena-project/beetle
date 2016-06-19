
import cronex
import dateutil.parser

from django.http import JsonResponse
from django.db.models import Q
from django.utils import timezone
from django.views.decorators.gzip import gzip_page
from django.views.decorators.http import require_GET

from .lookup import query_can_map_static
from .models import DynamicAuth, AdminAuth, UserAuth, \
	PasscodeAuth, NetworkAuth, Exclusive

from beetle.models import Contact
from network.models import ServiceInstance, CharInstance
from network.lookup import get_gateway_and_device_helper

from utils.decorators import require_api_port

def __get_dynamic_auth(rule, principal):
	result = []
	for auth in DynamicAuth.objects.filter(rule=rule).order_by("priority"):
		auth_obj = {
			"when" : auth.require_when,
		}
		if isinstance(auth, NetworkAuth):
			auth_obj["type"] = "network"
			auth_obj["ip"] = auth.ip_address
			auth_obj["priv"] = auth.is_private
		elif isinstance(auth, PasscodeAuth):
			auth_obj["type"] = "passcode"
		elif isinstance(auth, UserAuth):
			auth_obj["type"] = "user"
			if principal.owner.id == Contact.NULL:
				# Rule is unsatisfiable: there is user to
				# authenticate
				return False, []
		elif isinstance(auth, AdminAuth):
			auth_obj["type"] = "admin"
			if auth.admin.id == Contact.NULL:
				# Rule is unsatisfiable: there is no admin
				return False, []
		else:
			continue
		result.append(auth_obj)

	return True, result

def __get_minimal_rules(rules, cached_relations):
	"""Returns the rules that are 'minimal'."""

	if rules.count() == 0:
		return rules

	not_minimal_ids = set()
	# TODO: this is not particularly efficient...
	# filling in upper triangular matrix of strict partial order.
	for lhs in rules.order_by("id"):
		for rhs in rules.filter(id__gte=lhs.id):
			key = (lhs.id, rhs.id)
			if key not in cached_relations:
				lhs_lte_rhs = lhs.static_lte(rhs)
				rhs_lte_lhs = rhs.static_lte(lhs)
				if lhs_lte_rhs and rhs_lte_lhs:
					cached_relations[key] = 0
				elif lhs_lte_rhs:
					cached_relations[key] = -1
				elif rhs_lte_lhs:
					cached_relations[key] = 1
				else:
					cached_relations[key] = 0
			cached_val = cached_relations[key]
			if cached_val == 0:
				pass
			elif cached_val > 0:
				not_minimal_ids.add(lhs.id)
			else:
				not_minimal_ids.add(rhs.id)

	rules = rules.exclude(id__in=not_minimal_ids)

	return rules

def __evaluate_cron(rule, timestamp, cached_cron):
	"""Returns whether the timestamp is in the trigger window of the rule"""

	if rule.id in cached_cron:
		return cached_cron[rule.id]

	result = False
	try:
		cron_str = str(rule.cron_expression)
		cron = cronex.CronExpression(cron_str)
		result = cron.check_trigger(timestamp.timetuple()[:5],
			utc_offset=timestamp.utcoffset().seconds / (60 ** 2))
	except Exception, err:
		print err
		result = False

	cached_cron[rule.id] = result
	return result

@require_api_port
@gzip_page
@require_GET
def query_can_map(request, from_gateway, from_id, to_gateway, to_id):
	"""Return whether fromId at fromGateway can connect to toId at toGateway"""

	if "timestamp" in request.GET:
		timestamp = dateutil.parser.parse(request.GET["timestamp"])
	else:
		timestamp = timezone.now()

	from_id = int(from_id)
	from_gateway, from_principal, _, conn_from_principal = \
		get_gateway_and_device_helper(from_gateway, from_id)

	to_id = int(to_id)
	to_gateway, to_principal, _, _ = \
		get_gateway_and_device_helper(to_gateway, to_id)

	can_map, applicable_rules = query_can_map_static(from_gateway,
		from_principal, to_gateway, to_principal, timestamp)

	response = {}
	if not can_map:
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

	# Cached already computed relations for a strict partial order
	cached_relations = {}
	cached_cron = {}

	services = {}
	rules = {}
	for service_instance in ServiceInstance.objects.filter(
		device_instance=conn_from_principal):
		service_rules = applicable_rules.filter(
			Q(service=service_instance.service) | Q(service__name="*"))

		service = service_instance.service

		for char_instance in CharInstance.objects.filter(
			service_instance=service_instance):

			characteristic = char_instance.characteristic

			char_rules = service_rules.filter(Q(characteristic=characteristic) |
				Q(characteristic__name="*"))

			for char_rule in __get_minimal_rules(char_rules, cached_relations):

				#####################################
				# Compute access per characteristic #
				#####################################

				# Evaluate the cron expression
				if not __evaluate_cron(char_rule, timestamp, cached_cron):
					continue

				# Access allowed
				if service.uuid not in services:
					services[service.uuid] = {}
				if characteristic.uuid not in services[service.uuid]:
					services[service.uuid][characteristic.uuid] = []
				if char_rule.id not in rules:
					satisfiable, dauth = __get_dynamic_auth(char_rule,
						to_principal)
					if not satisfiable:
						continue

					if char_rule.exclusive:
 						exclusive_id = char_rule.exclusive.id
 					else:
 						exclusive_id = Exclusive.NULL

					# Put the rule in the result
					rules[char_rule.id] = {
						"prop" : char_rule.properties,
						"excl" : exclusive_id,
						"int" : char_rule.integrity,
						"enc" : char_rule.encryption,
						"lease" : (timestamp + char_rule.lease_duration
							).strftime("%s"),
						"dauth" : dauth,
					}

				services[service.uuid][characteristic.uuid].append(
					char_rule.id)

	if not rules:
		response["result"] = False
	else:
		response["result"] = True
		response["access"] = {
			"rules" : rules,
			"services" : services,
		}
	return JsonResponse(response)
