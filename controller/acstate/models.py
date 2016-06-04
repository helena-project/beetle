from __future__ import unicode_literals

from django.db import models
from django.utils import timezone

from beetle.models import Principal
from access.models import Rule
from network.models import ConnectedDevice

from dateutil.relativedelta import relativedelta

# Create your models here.

class AdminAuthInstance(models.Model):
	"""Instance of authorization from admin."""

	class Meta:
		verbose_name = "AdminAuth state"
		verbose_name_plural = "AdminAuth state"
		unique_together = ("rule", "principal")

	rule = models.ForeignKey("access.Rule")
	principal = models.ForeignKey("beetle.Principal")
	allow = models.BooleanField(default=True)

	timestamp = models.DateTimeField(auto_now_add=True)
	expire = models.DateTimeField(default=timezone.now)

	def __unicode__(self):
		return "%d %s" % (self.rule.id, self.principal.name)

class UserAuthInstance(models.Model):
	"""Instance of authentication of user."""

	class Meta:
		verbose_name = "UserAuth state"
		verbose_name_plural = "UserAuth state"
		unique_together = ("rule", "principal")

	rule = models.ForeignKey("access.Rule")
	principal = models.ForeignKey("beetle.Principal")
	allow = models.BooleanField(default=True)

	timestamp = models.DateTimeField(auto_now_add=True)
	expire = models.DateTimeField(default=timezone.now)

	def __unicode__(self):
		return "%d %s" % (self.rule.id, self.principal.name)

class PasscodeAuthInstance(models.Model):
	"""Instance of password authentication."""

	class Meta:
		verbose_name = "PasscodeAuth state"
		verbose_name_plural = "PasscodeAuth state"
		unique_together = ("rule", "principal")

	rule = models.ForeignKey("access.Rule")
	principal = models.ForeignKey("beetle.Principal")
	timestamp = models.DateTimeField(auto_now_add=True)
	expire = models.DateTimeField(default=timezone.now)

	def __unicode__(self):
		return "%d %s" % (self.rule.id, self.principal.name)

class ExclusiveLease(models.Model):
	"""A lease of exclusivity"""

	group = models.OneToOneField("access.Exclusive")
	device_instance = models.ForeignKey("network.ConnectedDevice")
	timestamp = models.DateTimeField(auto_now_add=True)
	expire = models.DateTimeField(default=timezone.now)

	def __unicode__(self):
		return "%d %s" % (self.group.id, self.device_instance.name)