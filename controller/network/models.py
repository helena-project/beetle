from __future__ import unicode_literals

from django.db import models

from datetime import timedelta

from beetle.models import VirtualDevice, Gateway
from gatt.models import Service, Characteristic

# Create your models here.

class ConnectedGateway(models.Model):
	"""A Beetle gateway"""

	class Meta:
		verbose_name = "Gateway Instance"
		verbose_name_plural = "Gateway Instances"

	DEFAULT_GATEWAY_TCP_SERVER_PORT = 3002 
	CONNECTION_TIMEOUT = timedelta(minutes=60)

	gateway = models.ForeignKey("beetle.Gateway")
	last_seen = models.DateTimeField(auto_now_add=True)
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
		verbose_name = "Device Instance"
		verbose_name_plural = "Device Instances"

	CONNECTION_TIMEOUT = timedelta(minutes=15)

	device = models.ForeignKey("beetle.VirtualDevice")
	gateway_instance = models.ForeignKey("ConnectedGateway")
	remote_id = models.IntegerField(
		help_text="id of the device on the gateway")
	last_seen = models.DateTimeField(auto_now_add=True)

	def __unicode__(self):
		return self.device.name + "@" + self.gateway_instance.gateway.name

class ServiceInstance(models.Model):
	"""An instance of a service"""

	device_instance = models.ForeignKey("ConnectedDevice")
	service = models.ForeignKey("gatt.Service") 

	def __unicode__(self):
		return self.service.__unicode__()

class CharInstance(models.Model):
	"""An instance of a characterisic"""
	
	device_instance = models.ForeignKey("ConnectedDevice")
	service_instance = models.ForeignKey("ServiceInstance")
	char = models.ForeignKey("gatt.Characteristic")

	def __unicode__(self):
		return self.char.__unicode__()