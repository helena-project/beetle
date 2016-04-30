from django.conf.urls import include, url

from . import views

urlpatterns = [
	url(r'^passcode/form/(?P<rule>\w+)',
		views.view_form_passcode_generic,
		name="passcode form"),
	url(r'^passcode/form/(?P<rule>\w+)/(?P<principal>[\w_\-@\' ]+)',
		views.view_form_passcode,
		name="passcode form"),
	url(r'^passcode/isLive/(?P<rule_id>\d+)/(?P<to_gateway>[\w_\-@\']+)/(?P<to_id>\d+)',
		views.query_passcode_liveness,
		name="query whether the passcode auth is live for the principal"),
	url(r'^admin/request/(?P<rule_id>\d+)/(?P<to_gateway>[\w_\-@\']+)/(?P<to_id>\d+)',
		views.request_admin_auth,
		name="send request to admin for authorization"),
	url(r'^user/request/(?P<rule_id>\d+)/(?P<to_gateway>[\w_\-@\']+)/(?P<to_id>\d+)',
		views.request_user_auth,
		name="send request to user for authorization"),
]