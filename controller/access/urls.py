from django.conf.urls import include, url

from . import views

urlpatterns = [
	url(r'^canMap/(?P<from_gatway>[\w_\-\']+)/(?P<from_id>\d+)/(?P<to_gateway>[\w_\-\']+)/(?P<to_id>\d+)$',
		views.query_can_map,
		name="can fromId at fromGateway be mapped to toId at toGateway?"),
]