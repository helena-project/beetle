from django.conf.urls import url

from .regex import uuid
from . import views

urlpatterns = [
	url(r'^service?/$', views.list_services),
	url(r'^char?/$', views.list_characteristics),
	url(r'^desc?/$', views.list_descriptors),
	url(r'^service/' + uuid("uuid") + r'$',
		views.find_service,
	 	name='find service by name'),
	url(r'^char/' + uuid("uuid") + r'$',
		views.find_characteristic,
		name='find characteristic by uuid'),
	url(r'^desc/' + uuid("uuid") + r'$',
		views.find_descriptor)
]
