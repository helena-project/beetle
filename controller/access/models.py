from __future__ import unicode_literals

from django.db import models
from django.utils import timezone
from django.core import serializers
from polymorphic.models import PolymorphicModel

from datetime import timedelta
from dateutil.relativedelta import relativedelta

from passlib.apps import django_context as pwd_context

from gatt.models import Service, Characteristic
from beetle.models import Principal, Gateway, Contact

# Create your models here.

def default_expire(self=None):
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

	description = models.CharField(
		max_length=500,
		default="", 
		blank=True,
		help_text="Description of the rule.")

	# fields queried using SQL
	from_principal = models.ForeignKey(
		"beetle.Principal", 
		related_name="rule_from",
		help_text="Application or peripheral acting as server.")
	from_gateway = models.ForeignKey(
		"beetle.Gateway", 
		related_name="rule_from_gateway",
		help_text="Gateway connected to server.")
	to_principal = models.ForeignKey(
		"beetle.Principal", 
		related_name="rule_to",
		help_text="Application or peripheral acting as client.")
	to_gateway = models.ForeignKey(
		"beetle.Gateway", 
		related_name="rule_to_gateway",
		help_text="Gateway connected to client.")

	service = models.ForeignKey(
		"gatt.Service", 
		default="")
	characteristic = models.ForeignKey(
		"gatt.Characteristic",
		default="")

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
	active = models.BooleanField(
		default=True,
		help_text="Rule will be considered?")

	def __unicode__(self):
		return self.name

class RuleException(models.Model):
	"""
	Deny, instead of allow, access. Used for attenuating existing rules.
	"""
	rule = models.ForeignKey(
		"Rule",
		help_text="Rule to invert")

	service = models.ForeignKey("gatt.Service")
	characteristic = models.ForeignKey("gatt.Characteristic")

	from_principal = models.ForeignKey(
		"beetle.Principal", 
		related_name="except_from",
		help_text="Application or peripheral acting as server.")
	from_gateway = models.ForeignKey(
		"beetle.Gateway", 
		related_name="except_from_gateway",
		help_text="Gateway connected to server.")
	to_principal = models.ForeignKey(
		"beetle.Principal", 
		related_name="except_to",
		help_text="Application or peripheral acting as client.")
	to_gateway = models.ForeignKey(
		"beetle.Gateway", 
		related_name="except_to_gateway",
		help_text="Gateway connected to client.")

	def __unicode__(self):
		return "(except) %s" % self.rule

class ExclusiveGroup(models.Model):
	"""
	Group rules by exclusive access.
	"""

	class Meta:
		verbose_name_plural = "Exclusive rule groups"

	description = models.CharField(
		max_length=500,
		help_text="Logical description of this group.")
	rules = models.ManyToManyField("Rule",
		blank=True,
		help_text="Rules which belong to this exclusive group.")

	def __unicode__(self):
		return self.description

#-------------------------------------------------------------------------------

class DynamicAuth(PolymorphicModel):
	"""
	Base class for dynamic rules.
	"""
	class Meta:
		verbose_name_plural = "Dynamic Auth"

	ON_MAP = 1
	ON_ACCESS = 2
	REQUIRE_WHEN_CHOICES = (
		(ON_MAP, "map"),
		(ON_ACCESS, "access"),
	)

	rule = models.ForeignKey("Rule")

	session_length = models.DurationField(
		default=timedelta(hours=1), 
		help_text="Time before reauthenticaton. Hint: HH:mm:ss")
	require_when = models.IntegerField(
		default=ON_MAP,
		choices=REQUIRE_WHEN_CHOICES,
		help_text="When to trigger authentication.")

#-------------------------------------------------------------------------------

class AdminAuth(DynamicAuth):
	"""
	Prompt the admin for permission.
	"""
	class Meta:
		verbose_name = "Admin"
		verbose_name_plural = "Admin Auth"

	message = models.CharField(
		max_length=100, 
		blank=True,
		help_text="Any additional message to present to the admin.")

	admin = models.ForeignKey("beetle.Contact",	
		help_text="User with authority over this rule")

	def __unicode__(self):
		return ""

class AdminAuthInstance(models.Model):
	"""
	Authenticated instance
	"""
	class Meta:
		verbose_name = "AdminAuth state"
		verbose_name_plural = "AdminAuth state"

	rule = models.ForeignKey("Rule")
	principal = models.ForeignKey("beetle.Principal")
	timestamp = models.DateTimeField(auto_now_add=True)
	expire = models.DateTimeField(default=timezone.now)

	def __unicode__(self):
		return "%d %s" % (self.rule.id, self.principal.name)

#-------------------------------------------------------------------------------

class SubjectAuth(DynamicAuth):
	"""
	Prompt the user for permission.
	"""
	class Meta:
		verbose_name = "Subject"
		verbose_name_plural = "Subject Auth"

	message = models.CharField(
		max_length=100, 
		blank=True,
		help_text="Any additional message to present to the user.")

	def __unicode__(self):
		return ""

#-------------------------------------------------------------------------------

class PasscodeAuth(DynamicAuth):
	"""
	Prompt user for a passcode.
	"""
	class Meta:
		verbose_name = "Passcode"
		verbose_name_plural = "Passcode Auth"

	code = models.CharField(
		max_length=200,
		blank=True,
		help_text="Enter a passcode for this rule.")
	chash = models.CharField(
		max_length=200,
		blank=True, 
		editable=False,
		help_text="Hashed passcode.")
	hint = models.CharField(
		max_length=500,
		blank=True,
		help_text="Passcode hint.")

	def save(self, *args, **kwargs):
		if self.code != "" and set(self.code) != set('*'):
			self.chash = pwd_context.encrypt(self.code)
			self.code = "*" * len(self.code)
		elif self.code == "":
			self.chash = ""
		super(PasscodeAuth, self).save(*args, **kwargs)

	def __unicode__(self):
		return ""

class PasscodeAuthInstance(models.Model):
	"""
	Authenticated instance
	"""
	class Meta:
		verbose_name = "PasscodeAuth state"
		verbose_name_plural = "PasscodeAuth state"

	rule = models.ForeignKey("Rule")
	principal = models.ForeignKey("beetle.Principal")
	timestamp = models.DateTimeField(auto_now_add=True)
	expire = models.DateTimeField(default=timezone.now)

	def __unicode__(self):
		return "%d %s" % (self.rule.id, self.principal.name)

#-------------------------------------------------------------------------------

class NetworkAuth(DynamicAuth):
	"""
	Is the client from a specific IP or subnet.
	"""

	class Meta:
		verbose_name = "Network"
		verbose_name_plural = "Network Auth"

	is_private = models.BooleanField(
		default=False,
		help_text="Allow access from any private IP.")
	ip_address = models.CharField(
		max_length=45, 
		default="127.0.0.1",
		help_text="IP address to be matched exactly.")

	def __unicode__(self):
		return self.ip_address