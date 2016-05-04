from django.contrib import admin
from django.core.validators import validate_email, ValidationError
from django import forms

import re
import json

from .models import Principal, Gateway, Contact

# Register your models here.

class PrincipalAdminForm(forms.ModelForm):
	def clean_members(self):
		members = self.cleaned_data["members"]
		ptype = self.cleaned_data["ptype"]
		if ptype != Principal.GROUP and len(members) > 0:
			raise ValidationError("Non group type cannot have any members.")
		return members
	def clean_owner(self):
		owner = self.cleaned_data["owner"]
		ptype = self.cleaned_data["ptype"]
		print owner, type(owner)
		if ptype == Principal.GROUP and owner.id != Contact.NULL:
			raise ValidationError("Groups cannot have owners. Must be null.")
		return owner

@admin.register(Principal)
class PrincipalAdmin(admin.ModelAdmin):
	form = PrincipalAdminForm
	list_display = ("name", "owner", "ptype", "get_member_list")
	search_fields = ("name", "owner", "get_member_list")
	list_filter = ("ptype",)

	def get_member_list(self, obj):
		if obj.ptype == Principal.GROUP:
			members = ["%s" % (m.name,) 
				for m in obj.members.all()]
			return json.dumps(members)
		else:
			return ""
	get_member_list.short_description = "members"

@admin.register(Gateway)
class GatewayAdmin(admin.ModelAdmin):
	list_display = ("name", "os")
	search_fields = ("name",)
	list_filter = ("os",)

class ContactAdminForm(forms.ModelForm):
	def clean_phone_number(self):
		number = self.cleaned_data["phone_number"]
		regex_match = re.match(r"^(\d{3})-?(\d{3})-?(\d{4})", number)
		if regex_match is None:
			raise forms.ValidationError("Invalid phone number.")
		else:
			return "%s-%s-%s" % (regex_match.group(1), regex_match.group(2), 
				regex_match.group(3))
	def clean_first_name(self):
		first_name = self.cleaned_data["first_name"]
		if re.match(r"^\w+", first_name) is None:
			raise forms.ValidationError("Invalid first name.")
		return first_name

	def clean_email_address(self):
		email_address = self.cleaned_data["email_address"]
		validate_email(email_address)
		return email_address

@admin.register(Contact)
class ContactAdmin(admin.ModelAdmin):
	form = ContactAdminForm
	
	list_display = ("first_name", "last_name", "phone_number", "email_address")
	search_fields = ("first_name", "last_name", "phone_number", "email_address")
