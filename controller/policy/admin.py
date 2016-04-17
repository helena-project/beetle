from django.contrib import admin
from django import forms
import cronex

# Register your models here.

from .models import Rule

from beetle.models import Entity, Gateway
from gatt.models import Service, Characteristic

class RuleAdminForm(forms.ModelForm):
	def clean_cron_expression(self):
		try:
			_ = cronex.CronExpression(str(self.cleaned_data["cron_expression"]))
		except:
			raise forms.ValidationError("Invalid cron expression.")
		return self.cleaned_data["cron_expression"]

def make_active(ruleadmin, request, queryset):
	queryset.update(active=True)
make_active.short_description = "Mark selected rules as active."

def make_inactive(ruleadmin, request, queryset):
	queryset.update(active=False)
make_inactive.short_description = "Mark selected rules as inactive."

@admin.register(Rule)
class RuleAdmin(admin.ModelAdmin):
	form = RuleAdminForm
	list_display = (
		"active",
		"server", 
		"server_gateway", 
		"client", 
		"client_gateway",
		"service",
		"characteristic",
		"properties",
		"cron_expression",
		"integrity",
		"encryption",
		"lease_duration",
		"start",
		"expire")
	actions = [make_active, make_inactive]
	search_fields = (
		"client", 
		"client_gateway", 
		"server", 
		"server_gateway",
		"service",
		"characteristic")
	list_filter = ("active", "integrity", "encryption")
	list_editable = ("active",)