from django.conf.urls import include, url

from . import views

urlpatterns = [
	url(r'^entity?/$', views.list_entities, name='list entities'),
	url(r'^gateway?/$', views.list_gateways, name='list gateways'),
	url(r'^entity/(?P<entity>[\w_\-\']+)$',
	 	views.find_entity, 
		name='find entity by name'),
	url(r'^gateway/(?P<gateway>[\w_\-\']+)$', 
		views.find_gateway,
	 	name='find gateway by name'),
]