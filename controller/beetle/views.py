from django.shortcuts import render
from django.http import JsonResponse, HttpResponse
from django.core import serializers
from django.views.decorators.http import require_GET

from .models import Gateway, Principal

# Create your views here.
@require_GET
def list_principals(request):
	""" 
	Return a list of application and peripheral names 
	"""
	return JsonResponse(
		[e.name for e in Principal.objects.all().order_by("name")], safe=False)

@require_GET
def list_gateways(request):
	""" 
	Return a list of gateway names 
	"""
	return JsonResponse(
		[gw.name for gw in Gateway.objects.all().order_by("name")], safe=False)

@require_GET
def find_principal(request, principal):
	""" 
	Return information about application or peripheral 
	"""
	principal = Principal.objects.get(name=principal)
	return JsonResponse({
		"name": principal.name,
		"type": principal.etype
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
