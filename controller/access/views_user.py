
from django.http import JsonResponse
from django.views.decorators.gzip import gzip_page
from django.views.decorators.http import require_GET

from .models import RuleException

@gzip_page
@require_GET
def view_rule_exceptions(request, rule):
	"""View in admin UI."""
	
	response = []
	for exception in RuleException.objects.filter(rule__name=rule):
		response.append({
			"from_principal" : exception.from_principal.name,
			"from_gateway" : exception.from_gateway.name,
			"to_principal" : exception.to_principal.name,
			"to_gateway" : exception.to_gateway.name,
			"service" : exception.service.name,
			"characteristic" : exception.characteristic.name,
		})
	return JsonResponse(response, safe=False)
