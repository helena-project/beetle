from django.conf.urls import include, url

from . import views

urlpatterns = [
	url(r'^(?P<rule>\w+)$',
		views.view_form_passcode_generic,
		name="passcode form"),
	url(r'^(?P<rule>\w+)/(?P<principal>[\w_\-@\' ]+)$',
		views.view_form_passcode,
		name="passcode form"),
]