from __future__ import unicode_literals

from django.db import models
from django.utils import timezone
from datetime import timedelta
from dateutil.relativedelta import relativedelta
from django.core import serializers

from gatt.models import Service, Characteristic
from beetle.models import Entity, Gateway

# Create your models here.

class DynamicAuth(models.Model):
	session = models.DurationField(
		default=timedelta(hours=1), 
		help_text="Time before reauthenticaton. Hint: HH:mm:ss")
	pass

class AdminAuth(DynamicAuth):
	pass

class UserAuth(DynamicAuth):
	pass

class PasscodeAuth(DynamicAuth):
	code = models.CharField(max_length=512)

class WifiAuth(DynamicAuth):
	ip_prefix = models.CharField(max_length=45)
	prefix_length = models.IntegerField()

def default_expire():
	return timezone.now() + relativedelta(years=1)

class Rule(models.Model):
	""" 
	A rule specifying when a GATT server and client may communicate 
	"""
	
	# fields queried using SQL
	from_entity = models.ForeignKey(
		"beetle.Entity", 
		related_name="rule_from",
		help_text="Application or peripheral acting as server.")
	from_gateway = models.ForeignKey(
		"beetle.Gateway", 
		related_name="rule_from_gateway",
		help_text="Gateway connected to server.")
	to_entity = models.ForeignKey(
		"beetle.Entity", 
		related_name="rule_to",
		help_text="Application or peripheral acting as client->")
	to_gateway = models.ForeignKey(
		"beetle.Gateway", 
		related_name="rule_to_gateway",
		help_text="Gateway connected to client->")

	service = models.ForeignKey("gatt.Service")
	characteristic = models.ForeignKey("gatt.Characteristic")

	# fields verified programatically
	cron_expression = models.CharField(
		max_length=100, 
		default="* * * * *",
		help_text="Standard crontab expression for when rule applies. \
					Format: Min Hour Day-of-Month Month Day-of-Week")

	# permissions and connection information
	properties = models.CharField(
		max_length=100, 
		blank=True,
		help_text="Hint: brwnix (broadcast, read, write, notify, indicate, exclusive)")
	integrity = models.BooleanField(
		help_text="Link layer integrity required.")
	encryption = models.BooleanField(
		help_text="Link layer encryption required.")
	lease_duration = models.DurationField(
		default=timedelta(minutes=15), 
		help_text="Hint: HH:mm:ss")

	# administrative fields
	start = models.DateTimeField(
		default=timezone.now)
	expire = models.DateTimeField(
		default=default_expire)
	active = models.BooleanField(default=True, 
		help_text="Rule will be considered?")

	def __unicode__(self):
		return "r_%d" % self.id