from django.contrib import admin

from .models import AdminAuthInstance, PasscodeAuthInstance, UserAuthInstance

# Register your models here.
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
