from __future__ import unicode_literals

from django.db import models

# Create your models here.
class Service(models.Model):
	"""A GATT service"""

	name = models.CharField(max_length=200)
	uuid = models.CharField(
		max_length=36,
		blank=True,
		primary_key=True,
		help_text="16 or 128-bit UUID.")
	ttype = models.CharField(
		max_length=200,
		blank=True,
		verbose_name="type")
	verified = models.BooleanField(
		default=False,
		help_text="Is this a standard service or added manually by a human?")

	def __unicode__(self):
		if self.name != "":
			return self.name
		else:
			return "<unk serv>." + self.uuid

class Characteristic(models.Model):
	"""A GATT characteristic"""

	name = models.CharField(max_length=200)
	uuid = models.CharField(
		max_length=36,
		blank=True,
		primary_key=True,
		help_text="16 or 128-bit UUID.")
	ttype = models.CharField(
		max_length=200,
		blank=True,
		verbose_name="type")
	verified = models.BooleanField(
		default=False,
		help_text="Is this a standard characteristic or added manually by a human?")

	def __unicode__(self):
		if self.name != "":
			return self.name
		else:
			return "<unk char>." + self.uuid

class Descriptor(models.Model):
	"""A GATT descriptor"""

	name = models.CharField(max_length=200)
	uuid = models.CharField(
		max_length=36,
		blank=True,
		primary_key=True,
		help_text="16 or 128-bit UUID.")
	ttype = models.CharField(
		max_length=200,
		blank=True,
		verbose_name="type")

	def __unicode__(self):
		if self.name != "":
			return self.name
		else:
			return "<unk desc>." + self.uuid
