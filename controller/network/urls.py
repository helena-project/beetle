from django.conf.urls import include, url

from . import views

urlpatterns = [
	url(r'^connect/(?P<gateway>[\w\-@\'\. ]+)$', 
		views.connect_gateway,
	 	name='connect a gateway'),
	url(r'^connect/(?P<gateway>[\w\-@\'\. ]+)/(?P<entity>[\w\-@\'\. ]+)/(?P<remote_id>\d+)$', 
		views.connect_entity, 
		name='connect an entity'),
	url(r'^connect/(?P<gateway>[\w\-@\'\. ]+)/(?P<remote_id>\d+)$', 
		views.disconnect_entity, 
		name='disconnect an entity'),
	url(r'^view/(?P<entity>[\w\-@\'\. ]+)$', 
		views.view_entity, 
		name='view the services and characteristics of an entity'),
	url(r'^discover/entities$', 
		views.discover_entities, 
		name='discover entities'),
	url(r'^discover/service/(?P<uuid>[\w\-]+)$', 
		views.discover_with_uuid, 
		{ "is_service" : True },
		name='discover entities with the service uuid'),
	url(r'^discover/char/(?P<uuid>[\w\-]+)$', 
		views.discover_with_uuid,  
		{ "is_service" : False },
		name='discover entities with the characteristic uuid'),
]