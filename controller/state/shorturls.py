from django.conf.urls import include, url

from access.regex import rule
from beetle.regex import device

from . import views_user as user

urlpatterns = [

	# User facing views

	url(r'^' + rule("rule") + r'/?$', 
		user.view_form_passcode_generic, 
		name="passcode form"),

	url(r'^' + rule("rule") + r'/' + device("principal") + r'$',
		user.view_form_passcode,
		name="passcode form"),
]