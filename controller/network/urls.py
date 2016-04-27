from django.conf.urls import include, url

from . import views

urlpatterns = [
	url(r'^connect/(?P<gateway>[\w\-@\'\. ]+)$', 
		views.connect_gateway,
	 	name='connect a gateway'),

	url(r'^connect/(?P<gateway>[\w\-@\'\. ]+)/(?P<principal>[\w\-@\'\. ]+)/(?P<remote_id>\d+)$', 
		views.connect_principal, 
		name='connect an principal'),
	url(r'^connect/(?P<gateway>[\w\-@\'\. ]+)/(?P<remote_id>\d+)$', 
		views.disconnect_principal, 
		name='disconnect an principal'),

	url(r'^find/gateway/(?P<gateway>[\w\-@\'\. ]+)$', 
		views.find_gateway,  
		name='find a gateway by name'),

	url(r'^view/(?P<principal>[\w\-@\'\. ]+)$', 
		views.view_principal, 
		name='view the services and characteristics of an principal'),
	
	url(r'^discover/principals$', 
		views.discover_principals, 
		name='discover principals'),
	url(r'^discover/service/(?P<uuid>[\w\-]+)$', 
		views.discover_with_uuid, 
		{ "is_service" : True },
		name='discover principals with the service uuid'),
	url(r'^discover/char/(?P<uuid>[\w\-]+)$', 
		views.discover_with_uuid,  
		{ "is_service" : False },
		name='discover principals with the characteristic uuid'),
]