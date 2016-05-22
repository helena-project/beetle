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
	service = models.ForeignKey(
		"gatt.Service", 
		default="")
	characteristic = models.ForeignKey(
		"gatt.Characteristic",
		default="")

	from_principal = models.ForeignKey(
		"beetle.Principal", 
		related_name="rule_from",
		verbose_name="Server Principal",
		help_text="Application or peripheral acting as server.")
	from_gateway = models.ForeignKey(
		"beetle.Gateway", 
		related_name="rule_from_gateway",
		help_text="Gateway connected to server.")
	to_principal = models.ForeignKey(
		"beetle.Principal", 
		related_name="rule_to",
		verbose_name="Client Principal",
		help_text="Application or peripheral acting as client.")
	to_gateway = models.ForeignKey(
		"beetle.Gateway", 
		related_name="rule_to_gateway",
		help_text="Gateway connected to client.")

	NUM_HIGH_PRIORITY_LEVELS = 1
	PRIORITY_CHOICES = ((0, "Normal"),) + tuple((i, "High-%d" % i) for i in xrange(1, NUM_HIGH_PRIORITY_LEVELS+1))

	priority = models.IntegerField(
		default=0,
		choices=PRIORITY_CHOICES)

	# fields verified programatically
	cron_expression = models.CharField(
		max_length=100, 
		default="* * * * *",
		verbose_name="Cron",
		help_text="Standard crontab expression for when rule applies. Format: Min Hour Day-of-Month Month Day-of-Week")

	# permissions and connection information
	properties = models.CharField(
		max_length=100, 
		blank=True,
		default="brwni",
		verbose_name="Props",
		help_text="Hint: brwni (broadcast, read, write, notify, indicate)")
	exclusive = models.ForeignKey("Exclusive",
		default=None,
		null=True,
		blank=True,
		verbose_name="Excl",
		help_text="Identifier to enforce exclusive access under.")
	integrity = models.BooleanField(
		default=True,
		verbose_name="MAC",
		help_text="Link layer integrity required.")
	encryption = models.BooleanField(
		default=True,
		verbose_name="ENC",
		help_text="Link layer encryption required.")
	lease_duration = models.DurationField(
		default=timedelta(minutes=15), 
		verbose_name="Lease",
		help_text="Maximum amount of time results may be cached. Hint: HH:mm:ss")

	# administrative fields
	start = models.DateTimeField(
		default=timezone.now)
	expire = models.DateTimeField(
		default=default_expire)
	active = models.BooleanField(
		default=True,
		help_text="Rule will be considered?")

	def domain_lte(self, rhs):
		"""
		Returns whether self is a subset of rhs.
		"""
		def _lte(a, b):
			return a == b or b.name == "*"
		def _lte_principal(a, b):
			return a == b or (b.ptype == Principal.GROUP and b.members.filter(
				name=a.name).exists()) or b.name == "*"

		is_domain_subset = True
		is_domain_subset &= _lte(self.service, rhs.service)
		is_domain_subset &= _lte(self.characteristic, rhs.characteristic)
		is_domain_subset &= _lte_principal(self.from_principal, 
			rhs.from_principal)
		is_domain_subset &= _lte(self.from_gateway, rhs.from_gateway)
		is_domain_subset &= _lte_principal(self.to_principal, rhs.to_principal)
		is_domain_subset &= _lte(self.to_gateway, rhs.to_gateway)

		return is_domain_subset

	def __unicode__(self):
		return self.name

class RuleException(models.Model):
	"""
	Deny, instead of allow, access. Used for attenuating existing rules.
	"""
	rule = models.ForeignKey(
		"Rule",
		help_text="Rule to invert")

	from_principal = models.ForeignKey(
		"beetle.Principal", 
		related_name="except_from",
		verbose_name="Server Principal",
		help_text="Application or peripheral acting as server.")
	from_gateway = models.ForeignKey(
		"beetle.Gateway", 
		related_name="except_from_gateway",
		help_text="Gateway connected to server.")
	to_principal = models.ForeignKey(
		"beetle.Principal", 
		related_name="except_to",
		verbose_name="Client Principal",
		help_text="Application or peripheral acting as client.")
	to_gateway = models.ForeignKey(
		"beetle.Gateway", 
		related_name="except_to_gateway",
		help_text="Gateway connected to client.")

	def __unicode__(self):
		return "(except) %s" % self.rule

class Exclusive(models.Model):
	"""
	Group rules by exclusive access.
	"""

	NULL = -1

	class Meta:
		verbose_name_plural = "Exclusive"

	description = models.CharField(
		max_length=200,
		blank=True,
		help_text="Logical description of this group.")

	default_lease = models.DurationField(
		default=timedelta(hours=1), 
		help_text="Length of the lease. Hint: HH:mm:ss")

	def __unicode__(self):
		return self.description

#------------------------------------------------------------------------------

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

	priority = models.IntegerField(
		default=0,
		editable=False,
		help_text="A hidden field to ensure envaluation order")

#------------------------------------------------------------------------------

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

	def save(self, *args, **kwargs):
		self.priority = 4
		super(AdminAuth, self).save(*args, **kwargs)

	def __unicode__(self):
		return ""

#------------------------------------------------------------------------------

class UserAuth(DynamicAuth):
	"""
	Prompt the user for permission.
	"""
	class Meta:
		verbose_name = "User Auth"
		verbose_name_plural = "User Auth"

	message = models.CharField(
		max_length=100, 
		blank=True,
		help_text="Any additional message to present to the user.")

	def save(self, *args, **kwargs):
		self.priority = 3
		super(UserAuth, self).save(*args, **kwargs)

	def __unicode__(self):
		return ""

#------------------------------------------------------------------------------

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
		self.priority = 2
		super(PasscodeAuth, self).save(*args, **kwargs)

	def __unicode__(self):
		return ""

#------------------------------------------------------------------------------

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

	def save(self, *args, **kwargs):
		self.priority = 1
		super(NetworkAuth, self).save(*args, **kwargs)

	def __unicode__(self):
		return self.ip_address