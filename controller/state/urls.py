from django.conf.urls import include, url

from access.regex import rule, rule_id, exclusive_id
from beetle.regex import gateway, device
from network.regex import device_id

from . import views_user as user
from . import views_api as api

urlpatterns = [
	
	# User facing

	url(r'^passcode/form/' + rule("rule") + r'$',
		user.view_form_passcode_generic,
		name="passcode form"),
	url(r'^passcode/form/' + rule("rule") + r'/' + device("principal") + r'$',
		user.view_form_passcode,
		name="passcode form"),
	
	# Internal APIs

	url(r'^passcode/isLive/' + rule_id('rule_id') + r'/' + gateway("to_gateway") 
		+ r'/' + device_id('to_id') + r"$",
		api.query_passcode_liveness,
		name="query whether the passcode auth is live for the principal"),

	url(r'^admin/request/' + rule_id('rule_id') + r'/' + gateway('to_gateway') 
		+ r'/' + device_id('to_id') + r"$",
		api.request_admin_auth,
		name="send request to admin for authorization"),

	url(r'^user/request/' + rule_id('rule_id') + r'/' + gateway('to_gateway') 
		+ r'/' + device_id('to_id') + r"$",
		api.request_user_auth,
		name="send request to user for authorization"),

	url(r'^exclusive/' + exclusive_id("exclusive_id") + r'/' 
		+ gateway('to_gateway') + r'/' + device_id('to_id') + r"$",
		api.request_exclusive_lease,
		name="send request to acquire/release lease"),
]