from django.contrib import admin
from django import forms
import cronex
import re

# Register your models here.

from .models import Rule, RuleException, AdminAuth, UserAuth, PasscodeAuth, \
	NetworkAuth, Exclusive

from beetle.models import Principal, Gateway
from gatt.models import Service, Characteristic

class RuleAdminForm(forms.ModelForm):
	description = forms.CharField(widget=forms.Textarea, required=False)

	def clean_cron_expression(self):
		try:
			_ = cronex.CronExpression(str(self.cleaned_data["cron_expression"]))
		except:
			raise forms.ValidationError("Invalid cron expression.")
		return self.cleaned_data["cron_expression"]
	def clean_name(self):
		name = self.cleaned_data["name"]
		if re.match(r"^\w+", name) is None:
			raise forms.ValidationError(
				"Name can only contain alphanumeric characters.")
		return name

def make_active(ruleadmin, request, queryset):
	queryset.update(active=True)
make_active.short_description = "Mark selected rules as active."

def make_inactive(ruleadmin, request, queryset):
	queryset.update(active=False)
make_inactive.short_description = "Mark selected rules as inactive."

class AlwaysChangedModelForm(forms.ModelForm):
	def has_changed(self):
		return True

class AdminAuthInline(admin.TabularInline):
	model = AdminAuth
	max_num = 1
	extra = 0
	form = AlwaysChangedModelForm

class UserAuthInline(admin.TabularInline):
	model = UserAuth
	max_num = 1
	extra = 0
	form = AlwaysChangedModelForm

class PasscodeAuthInline(admin.TabularInline):
	model = PasscodeAuth
	max_num = 1
	extra = 0
	form = AlwaysChangedModelForm

class NetworkAuthInline(admin.TabularInline):
	model = NetworkAuth
	max_num = 1
	extra = 0
	form = AlwaysChangedModelForm

class RuleExceptionInline(admin.TabularInline):
	model = RuleException
	extra = 1

@admin.register(Rule)
class RuleAdmin(admin.ModelAdmin):
	form = RuleAdminForm
	save_as = True
	list_display = (
		"name",
		"id",
		# "get_description",
		"priority",
		"service",
		"characteristic",
		"from_principal", 
		"from_gateway", 
		"to_principal", 
		"to_gateway",
		# "get_exceptions_link",
		"cron_expression",
		"properties",
		"exclusive",
		"integrity",
		"encryption",
		"lease_duration",
		# "start",
		# "expire",
		"active")
	actions = [make_active, make_inactive]
	search_fields = (
		"to_principal", 
		"to_gateway", 
		"from_principal", 
		"from_gateway",
		"service",
		"characteristic")
	list_filter = ("active", "priority", "integrity", "encryption")
	inlines = (
		RuleExceptionInline,
		AdminAuthInline, 
		UserAuthInline,
		PasscodeAuthInline, 
		NetworkAuthInline,)

	def get_description(self, obj):
		if len(obj.description) > 40:
			return obj.description[:40] + "..."
		else:
			return obj.description 
	get_description.short_description = "description"

	def get_exceptions_link(self, obj):
		return '<a href="/access/view/rule/%s/except" target="_blank">link</a>' % (obj.name,)
	get_exceptions_link.short_description = "except"
	get_exceptions_link.allow_tags = True

@admin.register(Exclusive)
class ExclusiveAdmin(admin.ModelAdmin):
	list_display = (
		"id",
		"description",
	)
	search_fields = (
		"id",
		"description",
	)

	def get_rule_list(self, obj):
		rules = Rule.objects.filter(exclusive=obj)
		return "<br>".join(rules)
	get_rule_list.short_description = "rules"
	get_rule_list.allow_tags = True