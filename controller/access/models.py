from __future__ import unicode_literals

from django.db import models
from django.utils import timezone
from datetime import timedelta
from dateutil.relativedelta import relativedelta
from django.core import serializers

from gatt.models import Service, Characteristic
from beetle.models import Entity, Gateway

# Create your models here.

def default_expire():
	return timezone.now() + relativedelta(years=1)

class Rule(models.Model):
	""" 
	A rule specifying when a GATT server and client may communicate 
	"""

	# human searchable name
	name = models.CharField(
		max_length=100, 
		unique=True,
		help_text="A human readable rule for searching and indexing.")

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
		help_text="Application or peripheral acting as client.")
	to_gateway = models.ForeignKey(
		"beetle.Gateway", 
		related_name="rule_to_gateway",
		help_text="Gateway connected to client.")

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
		default="brwni",
		help_text="Hint: brwni (broadcast, read, write, notify, indicate)")
	exclusive = models.BooleanField(
		default=False,
		help_text="Enforce exclusive access.")
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
		return "(rule) %d" % self.id

class RuleException(models.Model):
	"""
	Deny, instead of allow, access
	"""
	rule = models.ForeignKey(
		"Rule",
		help_text="Rule to invert")
	from_entity = models.ForeignKey(
		"beetle.Entity", 
		related_name="except_from",
		help_text="Application or peripheral acting as server.")
	from_gateway = models.ForeignKey(
		"beetle.Gateway", 
		related_name="except_from_gateway",
		help_text="Gateway connected to server.")
	to_entity = models.ForeignKey(
		"beetle.Entity", 
		related_name="except_to",
		help_text="Application or peripheral acting as client.")
	to_gateway = models.ForeignKey(
		"beetle.Gateway", 
		related_name="except_to_gateway",
		help_text="Gateway connected to client.")

	# TODO: allow fine exclusions based on service and characteristic

	def __unicode__(self):
		return "(except) %d" % self.id

class ExclusiveGroup(models.Model):
	"""
	Group rules by exclusive access.
	"""
	description = models.CharField(
		max_length=500,
		help_text="Logical description of this group.")
	rules = models.ManyToManyField("Rule",
		blank=True,
		help_text="Rules which belong to this exclusive group.")

	def __unicode__(self):
		return self.description

class User(models.Model):
	"""
	A human user who can be used to answer auth requests.
	"""
	class Meta:
		unique_together = (("first_name", "last_name"),)

	first_name = models.CharField(max_length=100)
	last_name = models.CharField(
		max_length=100, 
		blank=True)
	phone_number = models.CharField(
		max_length=100)
	email_address = models.CharField(
		max_length=100, 
		blank=True)
	gateways = models.ManyToManyField(
		"beetle.Gateway",
		blank=True,
		help_text="Gateways that this user may provide credentials from.")

	def __unicode__(self):
		return first_name + " " + last_name

class DynamicAuth(models.Model):
	"""
	Base class for dynamic rules.
	"""
	class Meta:
		verbose_name = "Factor"
		verbose_name_plural = "Dynamic Auth"

	ON_CONNECT = "OnConnect"
	ON_USE = "OnUse"
	DELAYED = "Delayed"
	REQUIRE_WHEN_CHOICES = (
		(ON_CONNECT, "OnConnect"),
		(ON_USE, "OnUse"),
		(DELAYED, "Delayed"),
	)

	rule = models.ForeignKey("Rule")

	session_len = models.DurationField(
		default=timedelta(hours=1), 
		help_text="Time before reauthenticaton. Hint: HH:mm:ss")
	require_when = models.CharField(
		max_length=100, 
		default=ON_USE,
		choices=REQUIRE_WHEN_CHOICES,
		help_text="When to trigger authentication.")

class AdminAuth(DynamicAuth):
	"""
	Prompt the admin for permission.
	"""
	class Meta:
		verbose_name = "Admin"
		verbose_name_plural = "Admin Auth"

	message = models.CharField(
		max_length=200, 
		blank=True,
		help_text="Any additional message to present to the admin.")

	admin = models.ForeignKey("User",	
		help_text="User with authority over this rule")

	def __unicode__(self):
		return ""

class SubjectAuth(DynamicAuth):
	"""
	Prompt the user for permission.
	"""
	class Meta:
		verbose_name = "Subject"
		verbose_name_plural = "Subject Auth"

	message = models.CharField(
		max_length=200, 
		blank=True,
		help_text="Any additional message to present to the user.")

	def __unicode__(self):
		return ""

class PasscodeAuth(DynamicAuth):
	"""
	Prompt user for a passcode.
	"""
	class Meta:
		verbose_name = "Passcode"
		verbose_name_plural = "Passcode Auth"

	code = models.CharField(max_length=512)
	salt = models.CharField(max_length=512)

	def __unicode__(self):
		return ""

class NetworkAuth(DynamicAuth):
	"""
	Is the client from a specific IP or subnet.
	"""

	class Meta:
		verbose_name = "Network"
		verbose_name_plural = "Network Auth"

	is_private = models.BooleanField(
		default=False,
		help_text="Allow access from any private IP, using the below as a mask.")
	ip_address = models.CharField(
		max_length=45, 
		default="0.0.0.0",
		help_text="IP address to be matched exactly.")

	def __unicode__(self):
		return self.ip_address