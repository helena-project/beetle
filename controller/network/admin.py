from django.contrib import admin

from .models import ConnectedGateway, ConnectedDevice, ServiceInstance, \
	CharInstance, DeviceMapping

# Register your models here.

class ConnectedDeviceInline(admin.TabularInline):
	model = ConnectedDevice
	fields = ("device", "gateway_instance",)
	readonly_fields = ("device", "gateway_instance",)

	def has_add_permission(self, request):
		return False

@admin.register(ConnectedGateway)
class ConnectedGatewayAdmin(admin.ModelAdmin):
	"""Show connected Beetle gateways and their devices"""

	list_display = ("get_gateway_name", "ip_address", "get_device_list",
		"is_connected", "last_updated", "session_token")
	filter_fields = ("is_connected",)
	search_fields = ("get_gateway_name", "ip_address", "get_device_list")
	inlines = (ConnectedDeviceInline,)
	readonly_fields = ("ip_address", "port", "gateway")

	def get_gateway_name(self, obj):
		return obj.gateway.name
	get_gateway_name.short_description = "gateway"
	get_gateway_name.admin_order_field = "gateway__name"

	def get_device_list(self, obj):
		ces = ["%d. %s" % (i + 1, ce.device.name)
			for i, ce in enumerate(ConnectedDevice.objects.filter(
				gateway_instance=obj))]
		return "<br>".join(ces)
	get_device_list.short_description = "connected"
	get_device_list.allow_tags = True

class ServiceInstanceInline(admin.TabularInline):
	model = ServiceInstance
	fields = ("service",)
	readonly_fields = fields
	def has_add_permission(self, request):
		return False

class CharInstanceInline(admin.TabularInline):
	model = CharInstance
	fields = ("service_instance", "characteristic")
	readonly_fields = fields
	def has_add_permission(self, request):
		return False

@admin.register(ConnectedDevice)
class ConnectedDeviceAdmin(admin.ModelAdmin):
	"""Show connected devices and their services and characteristics"""

	list_display = ("get_device_name", "get_gateway_name", "remote_id",
		"get_device_link",)
	search_fields = ("get_device_name", "get_gateway_name", "remote_id")
	list_filter = ("gateway_instance",)
	inlines = (ServiceInstanceInline, CharInstanceInline)
	readonly_fields = ("device", "gateway_instance", "remote_id",)

	def get_device_name(self, obj):
		return obj.device.name
	get_device_name.short_description = "device"
	get_device_name.admin_order_field = "device__name"

	def get_gateway_name(self, obj):
		return obj.gateway_instance.gateway.name
	get_gateway_name.short_description = "gateway"
	get_gateway_name.admin_order_field = "gateway__gateway__name"

	def get_device_link(self, obj):
		return '<a href="/network/view/%s" target="_blank">link</a>' % (
			obj.device.name,)
	get_device_link.short_description = "detail"
	get_device_link.allow_tags = True

@admin.register(DeviceMapping)
class DeviceMappingAdmin(admin.ModelAdmin):
	"""Show which devices have handles mapped"""

	list_display = ("from_device", "to_device", "timestamp")
	search_fields = ("from_device", "to_device",)
	readonly_fields = ("from_device", "to_device",)