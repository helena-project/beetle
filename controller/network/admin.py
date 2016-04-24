from django.contrib import admin

from .models import ConnectedGateway, ConnectedEntity, ServiceInstance, \
	CharInstance, ExclusiveLease

# Register your models here.

class ConnectedEntityInline(admin.TabularInline):
	model = ConnectedEntity
	fields = ("get_entity_name", "get_gateway_name", "last_seen")
	readonly_fields = fields

	def has_add_permission(self, request):
		return False

	def get_entity_name(self, obj):
		return obj.entity.name
	get_entity_name.short_description = "entity"
	get_entity_name.admin_order_field = "entity__name"

	def get_gateway_name(self, obj):
		return obj.gateway.gateway.name
	get_gateway_name.short_description = "gateway"
	get_gateway_name.admin_order_field = "gateway__gateway__name"

@admin.register(ConnectedGateway)
class ConnectedGatewayAdmin(admin.ModelAdmin):
	list_display = ("get_gateway_name", "last_seen", "ip_address", "get_entity_list")
	search_fields = ("get_gateway_name", "ip_address", "get_entity_list")
	ordering = ("last_seen",)
	inlines = (ConnectedEntityInline,)

	def get_gateway_name(self, obj):
		return obj.gateway.name
	get_gateway_name.short_description = "gateway"
	get_gateway_name.admin_order_field = "gateway__name"

	def get_entity_list(self, obj):
		ces = ["%d. %s" % (i + 1, ce.entity.name) 
			for i, ce in enumerate(ConnectedEntity.objects.filter(gateway=obj))]
		return "<br>".join(ces)
	get_entity_list.short_description = "connected"
	get_entity_list.allow_tags = True

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

@admin.register(ConnectedEntity)
class ConnectedEntityAdmin(admin.ModelAdmin):
	list_display = ("get_entity_name", "get_gateway_name", "get_entity_link", "last_seen")
	search_fields = ("get_entity_name", "get_gateway_name")
	list_filter = ("gateway",)
	ordering = ('last_seen',)
	inlines = (ServiceInstanceInline, CharInstanceInline)

	def get_entity_name(self, obj):
		return obj.entity.name
	get_entity_name.short_description = "entity"
	get_entity_name.admin_order_field = "entity__name"

	def get_gateway_name(self, obj):
		return obj.gateway.gateway.name
	get_gateway_name.short_description = "gateway"
	get_gateway_name.admin_order_field = "gateway__gateway__name"

	def get_entity_link(self, obj):
		return '<a href="/network/view/%s" target="_blank">link</a>' % (obj.entity.name)
	get_entity_link.short_description = "detail"
	get_entity_link.allow_tags = True

@admin.register(ExclusiveLease)
class ExclusiveLeaseAdmin(admin.ModelAdmin):
	list_display = ("gateway", "group",)
	search_fields = ("gateway", "group")