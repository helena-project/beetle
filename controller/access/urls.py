from django.conf.urls import include, url

from . import views

urlpatterns = [
	url(r'^canMap/(?P<from_gateway>[\w_\-@\']+)/(?P<from_id>\d+)/(?P<to_gateway>[\w_\-@\']+)/(?P<to_id>\d+)$',
		views.query_can_map,
		name="can fromId at fromGateway be mapped to toId at toGateway?"),
	
	url(r'^view/rule/(?P<rule>\w+)/except$',
		views.view_rule_exceptions,
		name="get list of exceptions"),

	url(r'^passcode/form/(?P<rule>\w+)/(?P<entity>[\w_\-@\' ]+)',
		views.view_form_passcode,
		name="passcode form"),
	url(r'^passcode/isLive/(?P<rule_id>\d+)/(?P<to_gateway>[\w_\-@\']+)/(?P<to_id>\d+)',
		views.query_passcode_liveness,
		name="query whether the passcode auth is live for the entity"),
]