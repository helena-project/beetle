from django.conf.urls import include, url

from . import views

urlpatterns = [
	url(r'^device?/$', views.list_devices, name='list devices'),
	url(r'^device/(?P<device>[\w_\-\']+)$', views.find_device, 
		name='find device by name'),

	url(r'^gateway?/$', views.list_gateways, name='list gateways'),
	url(r'^gateway/(?P<gateway>[\w_\-\']+)$', views.find_gateway,
	 	name='find gateway by name'),
]