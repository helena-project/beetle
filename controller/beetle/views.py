from django.shortcuts import render
from django.http import JsonResponse, HttpResponse
from django.core import serializers
from django.views.decorators.http import require_GET

from .models import Gateway, Entity

# Create your views here.
@require_GET
def list_entities(request):
	""" 
	Return a list of application and peripheral names 
	"""
	return JsonResponse(
		[e.name for e in Entity.objects.all().order_by("name")], safe=False)

@require_GET
def list_gateways(request):
	""" 
	Return a list of gateway names 
	"""
	return JsonResponse(
		[gw.name for gw in Gateway.objects.all().order_by("name")], safe=False)

@require_GET
def find_entity(request, entity):
	""" 
	Return information about application or peripheral 
	"""
	entity = Entity.objects.get(name=entity)
	return JsonResponse({
		"name": entity.name,
		"type": entity.etype
	})

@require_GET
def find_gateway(request, gateway):
	""" 
	Return information about gateway 
	"""
	gateway = Gateway.objects.get(name=gateway)
	return JsonResponse({
		"name": gateway.name,
		"os": gateway.os,
		"trusted": gateway.trusted
	})
