from django.conf.urls import url

from beetle.regex import gateway
from network.regex import device_id

from .regex import rule
from . import views_api as api
from . import views_user as user

urlpatterns = [

	# User facing
	
	url(r'^view/rule/' + rule('rule') + r'/except$',
		user.view_rule_exceptions,
		name="get list of exceptions"),

	url(r'^view/clients/', user.view_allowed_clients, 
		name="get a list of clients that can access the server"),

	# Internal APIs

	url(r'^canMap/' + gateway("from_gateway") + r'/' + device_id("from_id") 
		+ r'/' + gateway("to_gateway") + r'/' + device_id("to_id") + r'$',
		api.query_can_map,
		name="can fromId at fromGateway be mapped to toId at toGateway?"),
]
