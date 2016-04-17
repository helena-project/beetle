from __future__ import unicode_literals

from django.db import models

# Create your models here.
class Service(models.Model):
	""" 
	A GATT service 
	"""
	name = models.CharField(max_length=200)
	uuid = models.CharField(
		max_length=36, 
		blank=True, 
		primary_key=True,
		help_text="16 or 128-bit UUID.")
	stype = models.CharField(max_length=200, blank=True)
	verified = models.BooleanField(
		default=False,
		help_text="Has this service been verified by a human?")

	def __unicode__(self):
		if self.name != "":
			return self.name
		else:
			return self.uuid

class Characteristic(models.Model):
	""" 
	A GATT characteristic 
	"""
	name = models.CharField(max_length=200)
	uuid = models.CharField(
		max_length=36, 
		blank=True, 
		primary_key=True,
		help_text="16 or 128-bit UUID.")
	ctype = models.CharField(max_length=200, blank=True)
	verified = models.BooleanField(
		default=False,
		help_text="Has this characteristic been verified by a human?")

	def __unicode__(self):
		if self.name != "":
			return self.name
		else:
			return self.uuid