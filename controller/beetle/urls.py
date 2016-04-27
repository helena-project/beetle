from django.conf.urls import include, url

from . import views

urlpatterns = [
	url(r'^principal?/$', views.list_principals, name='list principals'),
	url(r'^gateway?/$', views.list_gateways, name='list gateways'),
	url(r'^principal/(?P<principal>[\w_\-\']+)$',
	 	views.find_principal, 
		name='find principal by name'),
	url(r'^gateway/(?P<gateway>[\w_\-\']+)$', 
		views.find_gateway,
	 	name='find gateway by name'),
]