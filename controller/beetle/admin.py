from django.contrib import admin
from django import forms

from .models import Entity, Gateway

# Register your models here.

class EntityAdminForm(forms.ModelForm):
	def __init__(self, *args, **kwargs):
		super(EntityAdminForm, self).__init__(*args, **kwargs)
		self.initial["verified"] = True 

@admin.register(Entity)
class EntityAdmin(admin.ModelAdmin):
	list_display = ("name", "etype", "verified")
	search_fields = ("name",)
	list_editable = ("verified",)
	list_filter = ("etype", "verified")

@admin.register(Gateway)
class GatewayAdmin(admin.ModelAdmin):
	list_display = ("name", "os", "trusted")
	search_fields = ("name", "trusted")
	list_filter = ("os", "trusted")

