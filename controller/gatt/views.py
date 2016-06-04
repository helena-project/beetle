from django.shortcuts import render
from django.http import JsonResponse, HttpResponse
from django.core import serializers
from django.views.decorators.http import require_GET

from .models import Service, Characteristic, Descriptor
from .shared import check_uuid, convert_uuid

# Create your views here.
@require_GET
def list_services(request):
	""" 
	Return a list of service names 
	"""
	return JsonResponse(
		[s.name for s in Service.objects.all().exclude(name="*").order_by(
			"name")], safe=False)

@require_GET
def list_characteristics(request):
	""" 
	Return a list of characteristic names 
	"""
	return JsonResponse(
		[c.name for c in Characteristic.objects.all().exclude(name="*"
			).order_by("name")], 
		safe=False)

@require_GET
def list_descriptors(request):
	""" 
	Return a list of descriptors names 
	"""
	return JsonResponse(
		[c.name for c in Descriptor.objects.all().order_by("name")], safe=False)

@require_GET
def find_service(request, uuid):
	""" 
	Return service info 
	"""
	if check_uuid(uuid) is None:
		return HttpResponse(status=400)
	uuid = convert_uuid(uuid)
	service = Service.objects.get(uuid=uuid)
	return JsonResponse({
		"name": service.name,
		"uuid": service.uuid,
		"type": service.ttype})

@require_GET
def find_characteristic(request, uuid):
	""" 
	Return characteristic info 
	"""
	if check_uuid(uuid) is None:
		return HttpResponse(status=400)
	uuid = convert_uuid(uuid)
	characteristic = Characteristic.objects.get(uuid=uuid)
	return JsonResponse({
		"name": characteristic.name,
		"uuid": characteristic.uuid,
		"type": characteristic.ttype})

@require_GET
def find_descriptor(request, uuid):
	""" 
	Return characteristic info 
	"""
	if check_uuid(uuid) is None:
		return HttpResponse(status=400)
	uuid = convert_uuid(uuid)
	descriptor = Descriptor.objects.get(uuid=uuid)
	return JsonResponse({
		"name": characteristic.name,
		"uuid": characteristic.uuid,
		"type": characteristic.ttype})
