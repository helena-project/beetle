
from django.http import HttpResponse

from main.constants import CONTROLLER_REST_API_PORT

def require_api_port(view):
	"""Restrict view to only the API port"""
	def wrapper(request, *args, **kwargs):
		if int(request.get_port()) != CONTROLLER_REST_API_PORT:
			return HttpResponse("Forbidden", status=403)
		return view(request, *args, **kwargs)
	return wrapper
