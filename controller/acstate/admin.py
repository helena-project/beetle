from django.contrib import admin
from django import forms

from .models import BeetleEmailAccount, AdminAuthInstance, PasscodeAuthInstance, UserAuthInstance, ExclusiveLease

from solo.admin import SingletonModelAdmin

# Register your models here.

class BeetleEmailAccountForm(forms.ModelForm):
	password = forms.CharField(widget=forms.PasswordInput())

class BeetleEmailAccountAdmin(SingletonModelAdmin):
	form = BeetleEmailAccountForm
	list_display = ("address",)
admin.site.register(BeetleEmailAccount, BeetleEmailAccountAdmin)

@admin.register(AdminAuthInstance)
class AdminAuthInstanceAdmin(admin.ModelAdmin):
	list_display = ("rule", "principal", "timestamp", "expire") 
	list_searchable = ("rule", "principal")
	list_filter = ("rule", "principal")
	readonly_fields = list_display

@admin.register(UserAuthInstance)
class UserAuthInstanceAdmin(admin.ModelAdmin):
	list_display = ("rule", "principal", "timestamp", "expire") 
	list_searchable = ("rule", "principal")
	list_filter = ("rule", "principal")
	readonly_fields = list_display

@admin.register(PasscodeAuthInstance)
class PasscodeAuthInstanceAdmin(admin.ModelAdmin):
	list_display = ("rule", "principal", "timestamp", "expire") 
	list_searchable = ("rule", "principal")
	list_filter = ("rule", "principal")
	readonly_fields = list_display

@admin.register(ExclusiveLease)
class ExclusiveLeaseAdmin(admin.ModelAdmin):
	list_display = ("principal", "group", "timestamp", "expire")
	search_fields = ("principal", "group")
	readonly_fields = list_display