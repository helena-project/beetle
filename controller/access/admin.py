from django.contrib import admin
from django import forms
import cronex
import re

# Register your models here.

from .models import User, Rule, RuleException, AdminAuth, SubjectAuth, \
	PasscodeAuth, NetworkAuth, ExclusiveGroup

from beetle.models import Entity, Gateway
from gatt.models import Service, Characteristic

class RuleAdminForm(forms.ModelForm):
	def clean_cron_expression(self):
		try:
			_ = cronex.CronExpression(str(self.cleaned_data["cron_expression"]))
		except:
			raise forms.ValidationError("Invalid cron expression.")
		return self.cleaned_data["cron_expression"]
	def clean_name(self):
		name = self.cleaned_data["name"]
		if re.match(r"^\w[\w ]*", name) is None:
			raise forms.ValidationError("Invalid name for rule")
		return name

def make_active(ruleadmin, request, queryset):
	queryset.update(active=True)
make_active.short_description = "Mark selected rules as active."

def make_inactive(ruleadmin, request, queryset):
	queryset.update(active=False)
make_inactive.short_description = "Mark selected rules as inactive."

class AdminAuthInline(admin.StackedInline):
    model = AdminAuth
    max_num = 1

class SubjectAuthInline(admin.StackedInline):
    model = SubjectAuth
    max_num = 1

class PasscodeAuthInline(admin.StackedInline):
    model = PasscodeAuth
    max_num = 1 

class NetworkAuthInline(admin.StackedInline):
    model = NetworkAuth
    extra = 3

class RuleExceptionInline(admin.TabularInline):
    model = RuleException
    extra = 1

@admin.register(Rule)
class RuleAdmin(admin.ModelAdmin):
	form = RuleAdminForm
	list_display = (
		"name",
		"from_entity", 
		"from_gateway", 
		"to_entity", 
		"to_gateway",
		"service",
		"characteristic",
		"get_exceptions_link",
		"cron_expression",
		"properties",
		"exclusive",
		"integrity",
		"encryption",
		"lease_duration",
		"start",
		"expire",
		"active")
	actions = [make_active, make_inactive]
	search_fields = (
		"to_entity", 
		"to_gateway", 
		"from_entity", 
		"from_gateway",
		"service",
		"characteristic")
	list_filter = ("active", "integrity", "encryption")
	inlines = (
		RuleExceptionInline,
		AdminAuthInline, 
		SubjectAuthInline, 
		PasscodeAuthInline, 
		NetworkAuthInline,)

	def get_exceptions_link(self, obj):
		return '<a href="/access/view/rule/%d/except" target="_blank">link</a>' % (obj.id,)
	get_exceptions_link.short_description = "except"
	get_exceptions_link.allow_tags = True

@admin.register(User)
class RuleAdmin(admin.ModelAdmin):
	list_display = (
		"first_name",
		"last_name",
		"phone_number",
	)
	search_fields = (
		"first_name",
		"last_name",
		"phone_number",
	)

@admin.register(ExclusiveGroup)
class ExclusiveGroupAdmin(admin.ModelAdmin):
	list_display = (
		"id",
		"description",
		"get_rule_list",
	)
	search_fields = (
		"id",
		"description",
		"get_rule_list",
	)

	def get_rule_list(self, obj):
		rules = ["%d. %s" % (rule.id, rule.name) for rule in obj.rules.all()]
		return "<br>".join(rules)
	get_rule_list.short_description = "rules"
	get_rule_list.allow_tags = True