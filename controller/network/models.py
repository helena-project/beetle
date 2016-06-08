from __future__ import unicode_literals

import os
import binascii

from django.db import models
from django.utils import timezone

from main.constants import GATEWAY_TCP_SERVER_PORT, SESSION_TOKEN_LEN

# Create your models here.

def _generate_session_token():
	"""Create a random session id"""
	return binascii.b2a_hex(os.urandom(SESSION_TOKEN_LEN))

class ConnectedGateway(models.Model):
	"""A Beetle gateway"""

	class Meta:
		verbose_name = "Gateway (Instance)"
		verbose_name_plural = "Gateways (Instance)"

	gateway = models.ForeignKey("beetle.BeetleGateway")
	ip_address = models.CharField(
		max_length=100, 
		help_text="IP address of the gateway.")
	port = models.IntegerField(
		default=GATEWAY_TCP_SERVER_PORT,
		help_text="TCP server port on the gateway.")

	session_token = models.CharField(
		max_length=100, editable=False,
		default=_generate_session_token,
		help_text="Hidden session token.")

	is_connected = models.BooleanField(
		default=False,
		help_text="Is manager connected.")

	last_updated = models.DateTimeField(
		default=timezone.now,
		editable=False)

	def save(self, *args, **kwargs):
		self.last_updated = timezone.now()
		super(ConnectedGateway, self).save(*args, **kwargs)

	def __unicode__(self):
		return "%s [%s]" % (self.gateway.name, self.ip_address)

class ConnectedDevice(models.Model):
	"""An app or peripheral"""

	class Meta:
		verbose_name = "Device (Instance)"
		verbose_name_plural = "Devices (Instance)"

	device = models.ForeignKey("beetle.VirtualDevice")
	gateway_instance = models.ForeignKey("ConnectedGateway")
	remote_id = models.IntegerField(
		help_text="Id of the device on the gateway.")

	interested_services = models.ManyToManyField(
		"gatt.Service", 
		blank=True,
		help_text="Services that the device has tried to find.")

	interested_characteristics = models.ManyToManyField(
		"gatt.Characteristic", 
		blank=True,
		help_text="Characteristics that the device has tried to find.")

	def __unicode__(self):
		return "%s [%s]" % (self.device.name, self.gateway_instance.gateway.name)

class ServiceInstance(models.Model):
	"""An instance of a service"""

	class Meta:
		verbose_name = "Service (Instance)"
		verbose_name_plural = "Services (Instance)"

	device_instance = models.ForeignKey("ConnectedDevice")
	service = models.ForeignKey("gatt.Service") 

	def __unicode__(self):
		return unicode(self.service)

class CharInstance(models.Model):
	"""An instance of a characteristic"""

	class Meta:
		verbose_name = "Characteristic (Instance)"
		verbose_name_plural = "Characteristics (Instance)"
	
	device_instance = models.ForeignKey("ConnectedDevice")
	service_instance = models.ForeignKey("ServiceInstance")
	characteristic = models.ForeignKey("gatt.Characteristic")

	def __unicode__(self):
		return unicode(self.characteristic)

class DeviceMapping(models.Model):
	"""Instance where two connected devices have mapped handles"""

	class Meta:
		verbose_name = "Mapping (Instance)"
		verbose_name_plural = "Mappings (Instance)"
		unique_together = (("from_device", "to_device"),)

	from_device = models.ForeignKey("ConnectedDevice", related_name="map_from")
	to_device = models.ForeignKey("ConnectedDevice", related_name="map_to")

	timestamp = models.DateTimeField(auto_now_add=True)

	def __unicode__(self):
		return "%s to %s" % (self.from_device.device, self.to_device)
		