from django.contrib import admin
from django import forms

from .models import Characteristic, Descriptor, Service
from .uuid import check_uuid, convert_uuid

# Register your models here.

class ServiceAdminForm(forms.ModelForm):
	def __init__(self, *args, **kwargs):
		super(ServiceAdminForm, self).__init__(*args, **kwargs)
		self.initial["verified"] = True 

	def clean_uuid(self):
		uuid = self.cleaned_data["uuid"]
		if check_uuid(uuid) is False:
			raise forms.ValidationError("Invalid service uuid.")
		return convert_uuid(uuid)

@admin.register(Service)
class ServiceAdmin(admin.ModelAdmin):
	form = ServiceAdminForm
	list_display = ("uuid", "name", "ttype", "verified")
	list_filter = ("verified",)
	list_editable = ("verified",)
	search_fields = ("uuid", "name", "ttype")
	ordering = ("uuid",)

class CharacteristicAdminForm(forms.ModelForm):
	def __init__(self, *args, **kwargs):
		super(CharacteristicAdminForm, self).__init__(*args, **kwargs)
		self.initial["verified"] = True 

	def clean_uuid(self):
		uuid = self.cleaned_data["uuid"]
		if check_uuid(uuid) is False:
			raise forms.ValidationError("Invalid characteristic uuid.")
		return convert_uuid(uuid)

@admin.register(Characteristic)
class CharacteristicAdmin(admin.ModelAdmin):
	form = CharacteristicAdminForm
	list_display = ("uuid", "name", "ttype", "verified")
	list_filter = ("verified",)
	list_editable = ("verified",)
	search_fields = ("uuid", "name", "ttype")
	ordering = ("uuid",)

@admin.register(Descriptor)
class DescriptorAdmin(admin.ModelAdmin):
	list_display = ("uuid", "name", "ttype")
	search_fields = ("uuid", "name", "ttype")
	ordering = ("uuid",)
