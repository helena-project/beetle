from django.shortcuts import render
from django.http import JsonResponse, HttpResponse
from django.db.models import Q
from django.core import serializers

from .models import Rule

from beetle.models import Entity, Gateway
from gatt.models import Service, Characteristic

from datetime import datetime
import dateutil.parser
import cronex

# Create your views here.

def _process_rules_helper(rules, timestamp=None):
	"""
	Evaluate the timestamps, and return a json response.
	"""
	allowed_rules = []

	for rule in rules:
		# verify the rule is active
		if timestamp is not None:
			cron_expression = cronex.CronExpression(str(rule.cron_expression))
			if not cron_expression.check_trigger(timestamp.timetuple()[:5]):
				continue

		allowed_rules.append(rule)

	def map_rule_to_json(rule):
		def td_to_millis(td):
			return td.days * 86400000 + td.seconds * 1000 + td.microseconds / 1000
		return {
			"ser": rule.service,
			"char": rule.characteristic,
			"prop": rule.properties,
			"lease": td_to_millis(rule.lease_duration),
			"int": rule.integrity,
			"enc": rule.encryption}

	return JsonResponse([map_rule_to_json(r) for r in allowed_rules], safe=False)

def query_gateways(request, server_gateway, client_gateway, timestamp=None):
	""" 
	Return rules which are active between two gateways 
	"""

	if timestamp is None and "timestamp" in request.GET:
		timestamp = dateutil.parser.parse(request.GET["timestamp"])
	else: 
		timestamp = datetime.now()

	server_gateway = Gateway.objects.get(name=server_gateway)
	client_gateway = Gateway.objects.get(name=client_gateway)

	rules = Rule.objects.filter(
		Q(server_gateway=server_gateway) | Q(server_gateway__name="*"),
		Q(client_gateway=client_gateway) | Q(client_gateway__name="*"),
		active=True)

	if timestamp is not None:
		rules = rules.filter(start__lte=timestamp, expire__gte=timestamp) \
			| rules.filter(expire__gte=None)

	return _process_rules_helper(rules, timestamp)

def query_entities(request, server, server_gateway, client, client_gateway, timestamp=None):
	""" 
	Return rules which are active between two entities 
	"""

	if timestamp is None and "timestamp" in request.GET:
		timestamp = dateutil.parser.parse(request.GET["timestamp"])

	server = Entity.objects.get(name=server)
	client = Entity.objects.get(name=client)
	server_gateway = Gateway.objects.get(name=server_gateway)
	client_gateway = Gateway.objects.get(name=client_gateway)

	rules = Rule.objects.filter(
		Q(server=server) | Q(server__name="*"),
		Q(server_gateway=server_gateway) | Q(server_gateway__name="*"),
		Q(client=client) | Q(client__name="*"),
		Q(client_gateway=client_gateway) | Q(client_gateway__name="*"),
		active=True)

	if timestamp is not None:
		rules = rules.filter(start__lte=timestamp, expire__gte=timestamp) \
			| rules.filter(expire__gte=None)

	return _process_rules_helper(rules, timestamp)