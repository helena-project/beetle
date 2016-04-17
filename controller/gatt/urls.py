from django.conf.urls import include, url

from . import views

urlpatterns = [
	url(r'^service?/$', views.list_services),
	url(r'^char?/$', views.list_characteristics),
	url(r'^service/(?P<uuid>[\w\-\']+)$', 
		views.find_service,
	 	name='find service by name'),
	url(r'^char/(?P<uuid>[\w\-\']+)$', 
		views.find_characteristic, 
		name='find characteristic by uuid'),
]