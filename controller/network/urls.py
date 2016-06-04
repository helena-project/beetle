from django.conf.urls import include, url

from . import views

urlpatterns = [
	url(r'^connect/(?P<gateway>[\w\-@\'\. ]+)$', 
		views.connect_gateway,
	 	name='connect a gateway'),

	url(r'^connect/(?P<gateway>[\w\-@\'\. ]+)/(?P<device>[\w\-@\'\. ]+)/(?P<remote_id>\d+)$', 
		views.connect_device, 
		name='connect an device'),
	url(r'^connect/(?P<gateway>[\w\-@\'\. ]+)/(?P<remote_id>\d+)$', 
		views.update_device, 
		name='update an device'),

	url(r'^find/gateway/(?P<gateway>[\w\-@\'\. ]+)$', 
		views.find_gateway,  
		name='find a gateway by name'),

	url(r'^view/(?P<device>[\w\-@\'\. ]+)$', 
		views.view_device, 
		name='view the services and characteristics of an device'),
	
	url(r'^discover/devices$', 
		views.discover_devices, 
		name='discover devices'),
	url(r'^discover/service/(?P<uuid>[\w\-]+)$', 
		views.discover_with_uuid, 
		{ "is_service" : True },
		name='discover devices with the service uuid'),
	url(r'^discover/char/(?P<uuid>[\w\-]+)$', 
		views.discover_with_uuid,  
		{ "is_service" : False },
		name='discover devices with the characteristic uuid'),
]