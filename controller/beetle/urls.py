from django.conf.urls import url

from . import regex
from . import views

urlpatterns = [

	# User facing views

	url(r'^device?/$', views.list_devices, name='list devices'),
	url(r'^device/' + regex.gateway("device") + r'$', views.find_device, 
		name='find device by name'),

	url(r'^gateway?/$', views.list_gateways, name='list gateways'),
	url(r'^gateway/' + regex.gateway('gateway') + r'$', views.find_gateway,
	 	name='find gateway by name'),
]
