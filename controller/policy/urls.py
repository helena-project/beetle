from django.conf.urls import include, url

from . import views

urlpatterns = [
	url(r'^gateways/(?P<server_gateway>[\w_\-\']+)/(?P<client_gateway>[\w_\-\']+)$', 
		views.query_gateways,
		name='pairwise query between gateways'),
	url(r'^entities/(?P<server>[\w_\-\']+)/(?P<server_gateway>[\w_\-\']+)/(?P<client>[\w_\-\']+)/(?P<client_gateway>[\w_\-\']+)$', 
		views.query_entities, 
		name='pairwise query between entities'),
]