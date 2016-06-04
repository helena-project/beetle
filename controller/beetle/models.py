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

	first_name = models.CharField(
		max_length=100)
	last_name = models.CharField(
		max_length=100, 
		blank=True)
	phone_number = models.CharField(
		max_length=100)
	email_address = models.CharField(
		max_length=100, 
		blank=True)
	
	def __unicode__(self):
		return self.first_name + " " + self.last_name

class Principal(PolymorphicModel):
	"""An application or peripheral device, using GATT"""
	
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

	auto_created = models.BooleanField(
		default=False,
		help_text="Added automatically by Beetle.")

class PrincipalGroup(Principal):
	"""A logical group of virtual devices"""
	class Meta:
		verbose_name = "Group (Principal)"
		verbose_name_plural = "Groups (Principal)"

	members = models.ManyToManyField(
		"VirtualDevice", 
		blank=True,
		help_text="Group members.")

class Gateway(models.Model):
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

	name = models.CharField(
		max_length=20, 
		primary_key=True)
	os = models.CharField(
		max_length=20, 
		default=LINUX, 
		choices=OS_CHOICES)

	def __unicode__(self):
		return self.name