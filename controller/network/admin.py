from django.contrib import admin

from .models import ConnectedGateway, ConnectedPrincipal, ServiceInstance, \
	CharInstance, ExclusiveLease

# Register your models here.

class ConnectedPrincipalInline(admin.TabularInline):
	model = ConnectedPrincipal
	fields = ("principal", "gateway", "last_seen")
	readonly_fields = ("principal", "gateway", "last_seen")

	def has_add_permission(self, request):
		return False

@admin.register(ConnectedGateway)
class ConnectedGatewayAdmin(admin.ModelAdmin):
	list_display = ("get_gateway_name", "last_seen", "ip_address", "get_principal_list")
	search_fields = ("get_gateway_name", "ip_address", "get_principal_list")
	ordering = ("last_seen",)
	inlines = (ConnectedPrincipalInline,)
	readonly_fields = ("last_seen", "ip_address", "port", "gateway")

	def get_gateway_name(self, obj):
		return obj.gateway.name
	get_gateway_name.short_description = "gateway"
	get_gateway_name.admin_order_field = "gateway__name"

	def get_principal_list(self, obj):
		ces = ["%d. %s" % (i + 1, ce.principal.name) 
			for i, ce in enumerate(ConnectedPrincipal.objects.filter(gateway=obj))]
		return "<br>".join(ces)
	get_principal_list.short_description = "connected"
	get_principal_list.allow_tags = True

class ServiceInstanceInline(admin.TabularInline):
	model = ServiceInstance
	fields = ("service",)
	readonly_fields = fields
	def has_add_permission(self, request):
		return False

class CharInstanceInline(admin.TabularInline):
	model = CharInstance
	fields = ("service", "char")
	readonly_fields = fields
	def has_add_permission(self, request):
		return False

@admin.register(ConnectedPrincipal)
class ConnectedPrincipalAdmin(admin.ModelAdmin):
	list_display = ("get_principal_name", "get_gateway_name", "remote_id", "get_principal_link", "last_seen")
	search_fields = ("get_principal_name", "get_gateway_name", "remote_id")
	list_filter = ("gateway",)
	ordering = ('last_seen',)
	inlines = (ServiceInstanceInline, CharInstanceInline)
	readonly_fields = ("principal", "gateway", "remote_id", "last_seen")

	def get_principal_name(self, obj):
		return obj.principal.name
	get_principal_name.short_description = "principal"
	get_principal_name.admin_order_field = "principal__name"

	def get_gateway_name(self, obj):
		return obj.gateway.gateway.name
	get_gateway_name.short_description = "gateway"
	get_gateway_name.admin_order_field = "gateway__gateway__name"

	def get_principal_link(self, obj):
		return '<a href="/network/view/%s" target="_blank">link</a>' % (obj.principal.name)
	get_principal_link.short_description = "detail"
	get_principal_link.allow_tags = True

@admin.register(ExclusiveLease)
class ExclusiveLeaseAdmin(admin.ModelAdmin):
	list_display = ("gateway", "group",)
	search_fields = ("gateway", "group")