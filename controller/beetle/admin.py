
import json
import re

from django.contrib import admin
from django.core.validators import validate_email, ValidationError
from django import forms
from polymorphic.admin import PolymorphicChildModelAdmin
from solo.admin import SingletonModelAdmin

from .models import PrincipalGroup, VirtualDevice, \
	BeetleGateway, GatewayGroup, Contact, BeetleEmailAccount

# Register your models here.

class BeetleEmailAccountForm(forms.ModelForm):
	def clean_address(self):
		address = self.cleaned_data["address"]
		validate_email(address)
		return address
	password = forms.CharField(widget=forms.PasswordInput())

@admin.register(BeetleEmailAccount)
class BeetleEmailAccountAdmin(SingletonModelAdmin):
	form = BeetleEmailAccountForm
	list_display = ("address",)

@admin.register(PrincipalGroup)
class PrincipalGroupAdmin(PolymorphicChildModelAdmin):
	base_model = PrincipalGroup
	list_display = ("name", "get_member_list")
	search_fields = ("name", "get_member_list")

	def get_member_list(self, obj):
		members = ["%s" % (m.name,) for m in obj.members.all()]
		return json.dumps(members)

	get_member_list.short_description = "members"

@admin.register(VirtualDevice)
class VirtualDeviceAdmin(PolymorphicChildModelAdmin):
	base_model = VirtualDevice
	list_display = ("name", "owner", "ttype", "auto_created")
	search_fields = ("name", "owner")
	list_filter = ("ttype",)
	list_editable = ("auto_created", "ttype", "owner")

@admin.register(BeetleGateway)
class BeetleGatewayAdmin(PolymorphicChildModelAdmin):
	base_model = BeetleGateway
	list_display = ("name", "os")
	search_fields = ("name",)
	list_filter = ("os",)

@admin.register(GatewayGroup)
class GatewayGroupAdmin(PolymorphicChildModelAdmin):
	base_model = GatewayGroup
	list_display = ("name", "get_member_list")
	search_fields = ("name", "get_member_list")

	def get_member_list(self, obj):
		members = ["%s" % (m.name,) for m in obj.members.all()]
		return json.dumps(members)

	get_member_list.short_description = "members"

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
