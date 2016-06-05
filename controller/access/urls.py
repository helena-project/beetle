from django.conf.urls import include, url

from . import regex

from beetle.regex import gateway
from network.regex import device_id

from . import views_api as api
from . import views_user as user

urlpatterns = [

	# User facing
	
	url(r'^view/rule/' + regex.rule('rule') + r'/except$',
		user.view_rule_exceptions,
		name="get list of exceptions"),

	# Internal APIs

	url(r'^canMap/' + gateway("from_gateway") + r'/' + device_id("from_id") 
		+ r'/' + gateway("to_gateway") + r'/' + device_id("to_id") + r'$',
		api.query_can_map,
		name="can fromId at fromGateway be mapped to toId at toGateway?"),
]