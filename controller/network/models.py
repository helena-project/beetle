from __future__ import unicode_literals

from django.db import models

from datetime import timedelta

from beetle.models import Entity, Gateway
from gatt.models import Service, Characteristic
from access.models import ExclusiveGroup

# Create your models here.

class ConnectedGateway(models.Model):
	"""
	A Beetle gateway
	"""
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

class ConnectedEntity(models.Model):
	"""
	An app or peripheral
	"""
	class Meta:
		verbose_name_plural = "Connected entities"

	CONNECTION_TIMEOUT = timedelta(minutes=15)

	entity = models.ForeignKey("beetle.Entity")
	gateway = models.ForeignKey("ConnectedGateway")
	remote_id = models.IntegerField(
		help_text="id of the device on the gateway")
	last_seen = models.DateTimeField(auto_now_add=True)

	def __unicode__(self):
		return self.entity.name + "@" + self.gateway.gateway.name

class ServiceInstance(models.Model):
	"""
	An instance of a service
	"""
	entity = models.ForeignKey("ConnectedEntity")
	service = models.ForeignKey("gatt.Service") 

	def __unicode__(self):
		return self.service.__unicode__()

class CharInstance(models.Model):
	"""
	An instance of a characterisic
	"""
	entity = models.ForeignKey("ConnectedEntity")
	service = models.ForeignKey("ServiceInstance")
	char = models.ForeignKey("gatt.Characteristic")

	def __unicode__(self):
		return self.char.__unicode__()

class ExclusiveLease(models.Model):
	"""
	A lease of exlusitivity
	"""
	group = models.ForeignKey("access.ExclusiveGroup")
	gateway = models.ForeignKey("ConnectedGateway")