from __future__ import unicode_literals

from django.db import models

# Create your models here.
class Entity(models.Model):
	""" 
	An application or peripheral device 
	"""
	
	class Meta:
		verbose_name_plural = 'Entities'

	# allowed types
	APP = "app"
	DEVICE = "device"
	UNKNOWN = "unknown"
	TYPE_CHOICES = (
		(APP, "app"),
		(DEVICE, "device"),
		(UNKNOWN, "unknown"),
	)

	name = models.CharField(max_length=100, primary_key=True)
	etype = models.CharField(max_length=20, choices=TYPE_CHOICES)
	verified = models.BooleanField(
		default=False,
		help_text="Has this entity been verified by a human?")

	def __unicode__(self):
		if self.etype == self.DEVICE:
			prefix = "(d) "
		elif self.etype == self.APP: 
			prefix = "(a) "
		else:
			prefix = ""
		return prefix + self.name

class Gateway(models.Model):
	""" 
	A gateway 
	"""

	# allowed types
	ANDROID = "android"
	LINUX = "linux"
	UNKNOWN = "unknown"
	OS_CHOICES = (
		(ANDROID, "android"),
		(LINUX, "linux"),
		(UNKNOWN, "unknown"),
	)

	name = models.CharField(max_length=20, primary_key=True)
	os = models.CharField(max_length=20, choices=OS_CHOICES)
	trusted = models.BooleanField(default=False)

	def __unicode__(self):
		return "(gw) " + self.name