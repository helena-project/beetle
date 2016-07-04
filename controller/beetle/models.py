from __future__ import unicode_literals

from django.db import models
from solo.models import SingletonModel
from polymorphic.models import PolymorphicModel

# Create your models here.

class BeetleEmailAccount(SingletonModel):
	"""Email to access SMS gateways"""

	class Meta:
		verbose_name = "Beetle Email"

	address = models.CharField(
		max_length=100,
		blank=True,
		help_text="The email to send SMS from.")
	password = models.CharField(
		max_length=100,
		blank=True)

	def __unicode__(self):
		return "Beetle Email"

class Contact(models.Model):
	"""A human user with contact information."""

	class Meta:
		unique_together = (("first_name", "last_name"),)

	# primary key of NULL
	NULL = 0

	AUTH_SMS = 1
	AUTH_EMAIL = 2
	AUTH_METHOD_CHOICES = (
		(AUTH_SMS, "sms"),
		(AUTH_EMAIL, "email"),
	)

	CARRIER_GATEWAY = ""
	CARRIER_ATT = "txt.att.net"
	CARRIER_TMOBILE = "tmomail.net"
	CARRIER_VERIZON = "vtext.com"
	CARRIER_SPRINT = "page.nextel.com"
	CARRIER_GATEWAY_CHOICES = (
		(CARRIER_GATEWAY, "default"),
		(CARRIER_ATT, "att"),
		(CARRIER_TMOBILE, "tmobile"),
		(CARRIER_VERIZON, "verizon"),
		(CARRIER_SPRINT, "sprint"),
	)

	first_name = models.CharField(
		max_length=100)
	last_name = models.CharField(
		max_length=100,
		blank=True)
	phone_number = models.CharField(
		max_length=100)
	carrier_gateway = models.CharField(
		choices=CARRIER_GATEWAY_CHOICES,
		default=CARRIER_GATEWAY)

	email_address = models.CharField(
		max_length=100,
		blank=True)
	auth_method = models.IntegerField(
		choices=AUTH_METHOD_CHOICES,
		default=AUTH_EMAIL)

	def __unicode__(self):
		return self.first_name + " " + self.last_name

class Principal(PolymorphicModel):
	"""An application, peripheral, or group"""

	class Meta:
		verbose_name = "Principal"
		verbose_name_plural = "Principals"

	name = models.CharField(
		max_length=100,
		primary_key=True)

	def __unicode__(self):
		return self.name

class VirtualDevice(Principal):
	"""An endpoint in Beetle"""

	class Meta:
		verbose_name = "Virtual device (Principal)"
		verbose_name_plural = "Virtual devices (Principal)"

	# allowed types
	LOCAL = "local"
	REMOTE = "remote"
	PERIPHERAL = "peripheral"
	UNKNOWN = "unknown"
	TYPE_CHOICES = (
		(LOCAL, "local"),
		(REMOTE, "remote"),
		(PERIPHERAL, "peripheral"),
		(UNKNOWN, "unknown"),
	)

	ttype = models.CharField(
		max_length=20,
		choices=TYPE_CHOICES,
		default=UNKNOWN,
		verbose_name="type")

	owner = models.ForeignKey(
		"Contact",
		default=Contact.NULL,
		help_text="Contact associated with this device.")

	serve_to = models.ManyToManyField(
		"Principal", blank=True, related_name="vd_server_to",
		help_text="Devices and groups to serve handles to " +
			"whenever possible and allowed.")

	discoverable_by = models.ManyToManyField(
		"Principal", blank=True, related_name="vd_discoverable_by",
		help_text="Devices and groups that may discover and connect in an "
			+ "ad-hoc manner.")

	auto_created = models.BooleanField(
		default=False,
		help_text="Added automatically by Beetle.")

	@property
	def client_to(self):
		groups = PrincipalGroup.objects.filter(members__in=[self])
		return VirtualDevice.objects.filter(serve_to__in=[self]) | \
			VirtualDevice.objects.filter(serve_to__in=groups)

	def __unicode__(self):
		return self.name

class PrincipalGroup(Principal):
	"""A logical group of virtual devices"""

	class Meta:
		verbose_name = "Group (Principal)"
		verbose_name_plural = "Groups (Principal)"

	members = models.ManyToManyField(
		"VirtualDevice",
		blank=True,
		help_text="Group members.")

	def __unicode__(self):
		return self.name + " (Group)"

class Gateway(PolymorphicModel):
	"""A beetle gateway or group"""

	name = models.CharField(
		max_length=20,
		primary_key=True)

	def __unicode__(self):
		return self.name

class BeetleGateway(Gateway):
	"""A gateway in the network, serving as a GATT translator"""

	class Meta:
		verbose_name = "Gateway"
		verbose_name_plural = "Gateways"

	# allowed types
	ANDROID = "android"
	LINUX = "linux"
	UNKNOWN = "unknown"
	OS_CHOICES = (
		(ANDROID, "android"),
		(LINUX, "linux"),
		(UNKNOWN, "unknown"),
	)

	os = models.CharField(
		max_length=20,
		default=LINUX,
		choices=OS_CHOICES)

class GatewayGroup(Gateway):
	"""A logical group of gateways"""

	class Meta:
		verbose_name = "Group (Gateway)"
		verbose_name_plural = "Groups (Gateway)"

	members = models.ManyToManyField(
		"BeetleGateway",
		blank=True,
		help_text="Group members.")

	def __unicode__(self):
		return self.name + " (Group)"
