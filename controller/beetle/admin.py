from django.contrib import admin
from django import forms

import re

from .models import Principal, Gateway, Contact

# Register your models here.

class PrincipalAdminForm(forms.ModelForm):
	def __init__(self, *args, **kwargs):
		super(PrincipalAdminForm, self).__init__(*args, **kwargs)
		self.initial["verified"] = True 

@admin.register(Principal)
class PrincipalAdmin(admin.ModelAdmin):
	list_display = ("name", "owner", "ptype", "verified")
	search_fields = ("name", "owner",)
	list_editable = ("verified",)
	list_filter = ("ptype", "verified")

@admin.register(Gateway)
class GatewayAdmin(admin.ModelAdmin):
	list_display = ("name", "os", "trusted")
	search_fields = ("name", "trusted")
	list_filter = ("os", "trusted")

class ContactAdminForm(forms.ModelForm):
	def clean_phone_number(self):
		number = self.cleaned_data["phone_number"]
		if re.match(r"^\d{3}-?\d{3}-?\d{4}", number) is None:
			raise forms.ValidationError("Invalid phone number.")
		return number
	def clean_first_name(self):
		first_name = self.cleaned_data["first_name"]
		if re.match(r"^\w+", first_name) is None:
			raise forms.ValidationError("Invalid first name.")
		return first_name

@admin.register(Contact)
class ContactAdmin(admin.ModelAdmin):
	form = ContactAdminForm
	
	list_display = ("first_name", "last_name", "phone_number", "email_address")
	search_fields = ("first_name", "last_name", "phone_number", "email_address")
