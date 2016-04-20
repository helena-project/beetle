from django.conf.urls import include, url

from . import views

urlpatterns = [
	url(r'^connect/(?P<gateway>[\w\-@\']+)$', 
		views.connect_gateway,
	 	name='connect a gateway'),
	url(r'^connect/(?P<gateway>[\w\-@\']+)/(?P<entity>[\w\-@\']+)/(?P<remote_id>\d+)$', 
		views.connect_entity, 
		name='connect an entity'),
	url(r'^connect/(?P<gateway>[\w\-@\']+)/(?P<remote_id>\d+)$', 
		views.disconnect_entity, 
		name='disconnect an entity'),
	url(r'^view/(?P<entity>[\w\-@\']+)$', 
		views.view_entity, 
		name='view the services and characteristics of an entity'),
	url(r'^discover/(?P<uuid>[\w\-@\']+)$', 
		views.discover_with_uuid, 
		name='discover entities with the uuid'),
]