"""policy_server URL Configuration

The `urlpatterns` list routes URLs to views. For more information please see:
    https://docs.djangoproject.com/en/1.9/topics/http/urls/
Examples:
Function views
    1. Add an import:  from my_app import views
    2. Add a URL to urlpatterns:  url(r'^$', views.home, name='home')
Class-based views
    1. Add an import:  from other_app.views import Home
    2. Add a URL to urlpatterns:  url(r'^$', Home.as_view(), name='home')
Including another URLconf
    1. Import the include() function: from django.conf.urls import url, include
    2. Add a URL to urlpatterns:  url(r'^blog/', include('blog.urls'))
"""
from django.conf.urls import include, url
from django.contrib import admin
from django.http import HttpResponseRedirect

admin.site.site_header = "Beetle Network Administration"
admin.site.site_title = "Beetle administration"
admin.site.index_title = "Beetle administration"

urlpatterns = [
    url(r'^gatt/', include('gatt.urls')),
    url(r'^beetle/', include('beetle.urls')),
    url(r'^access/', include('access.urls')),
    url(r'^network/', include('network.urls')),
    url(r'^state/', include('state.urls')),
    url(r'^code/', include('state.shorturls')),
    url(r'^admin/', admin.site.urls),
    url(r'^$', lambda r: HttpResponseRedirect('admin/')),
]
