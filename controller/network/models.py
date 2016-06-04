from __future__ import unicode_literals

from django.db import models

from datetime import timedelta

from beetle.models import VirtualDevice, Gateway
from gatt.models import Service, Characteristic

# Create your models here.

class ConnectedGateway(models.Model):
	"""A Beetle gateway"""

	class Meta:
		verbose_name = "Gateway (Instance)"
		verbose_name_plural = "Gateways (Instance)"

	DEFAULT_GATEWAY_TCP_SERVER_PORT = 3002 
	CONNECTION_TIMEOUT = timedelta(minutes=60)

	gateway = models.ForeignKey("beetle.Gateway")
	ip_address = models.CharField(
		max_length=100, 
		help_text="IP address of the gateway")
	port = models.IntegerField(
		default=DEFAULT_GATEWAY_TCP_SERVER_PORT,
		help_text="TCP server port on the gateway")

	def __unicode__(self):
		return self.gateway.name + "@" + self.ip_address

class ConnectedDevice(models.Model):
	"""An app or peripheral"""

	class Meta:
		verbose_name = "Device (Instance)"
		verbose_name_plural = "Devices (Instance)"

	CONNECTION_TIMEOUT = timedelta(minutes=15)

	device = models.ForeignKey("beetle.VirtualDevice")
	gateway_instance = models.ForeignKey("ConnectedGateway")
	remote_id = models.IntegerField(
		help_text="id of the device on the gateway")

	def __unicode__(self):
		return self.device.name + "@" + self.gateway_instance.gateway.name

class ServiceInstance(models.Model):
	"""An instance of a service"""

	class Meta:
		verbose_name = "Service (Instance)"
		verbose_name_plural = "Services (Instance)"

	device_instance = models.ForeignKey("ConnectedDevice")
	service = models.ForeignKey("gatt.Service") 

	def __unicode__(self):
		return self.service.__unicode__()

class CharInstance(models.Model):
	"""An instance of a characteristic"""

	class Meta:
		verbose_name = "Characteristic (Instance)"
		verbose_name_plural = "Characteristics (Instance)"
	
	device_instance = models.ForeignKey("ConnectedDevice")
	service_instance = models.ForeignKey("ServiceInstance")
	characteristic = models.ForeignKey("gatt.Characteristic")

	def __unicode__(self):
		return self.characteristic.__unicode__()

class DeviceMapping(models.Model):
	"""Instance where two connected devices have mapped handles"""

	class Meta:
		verbose_name = "Mapping (Instance)"
		verbose_name_plural = verbose_name

	from_device = models.ForeignKey("ConnectedDevice", related_name="map_from")
	to_device = models.ForeignKey("ConnectedDevice", related_name="map_to")

	timestamp = models.DateTimeField(auto_now_add=True)


	def __unicode__(self):
		return from_device.device.__unicode__() + " to " + to.device.__unicode__()