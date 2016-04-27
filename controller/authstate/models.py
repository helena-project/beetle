from __future__ import unicode_literals

from django.db import models
from django.utils import timezone

from beetle.models import Principal
from access.models import Rule

# Create your models here.

class AdminAuthInstance(models.Model):
	"""
	Authenticated instance
	"""
	class Meta:
		verbose_name = "AdminAuth state"
		verbose_name_plural = "AdminAuth state"

	rule = models.ForeignKey("access.Rule")
	principal = models.ForeignKey("beetle.Principal")
	timestamp = models.DateTimeField(auto_now_add=True)
	expire = models.DateTimeField(default=timezone.now)

	def __unicode__(self):
		return "%d %s" % (self.rule.id, self.principal.name)

class PasscodeAuthInstance(models.Model):
	"""
	Authenticated instance
	"""
	class Meta:
		verbose_name = "PasscodeAuth state"
		verbose_name_plural = "PasscodeAuth state"

	rule = models.ForeignKey("access.Rule")
	principal = models.ForeignKey("beetle.Principal")
	timestamp = models.DateTimeField(auto_now_add=True)
	expire = models.DateTimeField(default=timezone.now)

	def __unicode__(self):
		return "%d %s" % (self.rule.id, self.principal.name)