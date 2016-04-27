from django.shortcuts import render, render_to_response
from django.http import JsonResponse, HttpResponse
from django.db.models import Q
from django.core import serializers
from django.utils import timezone
from django.views.decorators.http import require_GET, require_http_methods
from django.template import RequestContext

from passlib.apps import django_context as pwd_context

from .models import AdminAuthInstance, PasscodeAuthInstance

from beetle.models import Principal, Gateway, Contact
from gatt.models import Service, Characteristic
from access.models import Rule, RuleException, DynamicAuth, PasscodeAuth, \
	AdminAuth, SubjectAuth, NetworkAuth
from network.models import ConnectedGateway, ConnectedPrincipal, \
	ServiceInstance, CharInstance
from network.shared import get_gateway_and_principal_helper, get_gateway_helper

import dateutil.parser
import cronex
import json
import base64

#-------------------------------------------------------------------------------

def _is_rule_exempt_helper(rule, principal):
	return bool(RuleException.objects.filter(
		Q(to_principal=principal) | Q(to_principal__name="*"),
		service__name="*", 
		characteristic__name="*",
		rule=rule))

#-------------------------------------------------------------------------------

@require_http_methods(["GET","POST"])
def view_form_passcode(request, rule, principal):
	context = RequestContext(request)

	try:
		rule = Rule.objects.get(name=rule)
		principal = Principal.objects.get(name=principal)
		passcode_auth = DynamicAuth.objects.instance_of(DynamicAuth).get(rule=rule)
	except Exception, err:
		context_dict = {
			"title" : "An error occurred...",
			"message" : str(err),
		}
		return render_to_response('authstate/empty_response.html', context_dict, context)

	now = timezone.now()

	context = RequestContext(request)
	context_dict = {
		"rule" : rule,
		"principal" : principal,
		"auth" : passcode_auth
	}

	if (rule.to_principal.name != "*" and rule.to_principal != principal) or \
		_is_rule_exempt_helper(rule, principal):
		#######################
		# Rule does not apply #
		#######################
		context_dict.update({
			"status" : {
				"a" : "Does Not Apply",
				"b" : "denied",
			}
		})
		return render_to_response('authstate/passcode_form_submit.html', 
			context_dict, context)

	if request.method == "GET":
		#######################
		# Requesting the form #
		#######################
		if passcode_auth.code == "":
			#############################
			# No passcode, grant access #
			#############################
			auth_instance, _ = PasscodeAuthInstance.objects.get_or_create(
				principal=principal, rule=rule)
			auth_instance.timestamp = now
			auth_instance.expire = now + passcode_auth.session_length
			auth_instance.save()

			context_dict.update({
				"status" : {
					"a" : "Success",
					"b" : "approved",
				}
			})
			return render_to_response('authstate/passcode_form_submit.html', 
				context_dict, context)
		else:
			############################
			# Render the password form #
			############################
			return render_to_response('authstate/passcode_form.html', 
				context_dict, context)
	elif request.method == "POST":
		#######################
		# Submitting the form #
		#######################
		code = request.POST["code"]
		allowed = False
		try:
			allowed = pwd_context.verify(code, passcode_auth.chash) 
		except:
			pass

		if not allowed:
			context_dict.update({
				"status" : {
					"a" : "Failure",
					"b" : "denied",
				}
			})
		else:
			auth_instance, _ = PasscodeAuthInstance.objects.get_or_create(
				principal=principal, rule=rule)
			auth_instance.timestamp = now
			auth_instance.expire = now + passcode_auth.session_length
			auth_instance.save()

			context_dict.update({
				"status" : {
					"a" : "Success",
					"b" : "approved",
				}
			})
		return render_to_response('authstate/passcode_form_submit.html', 
			context_dict, context)
	else:
		return HttpResponse(status=403)

@require_GET
def query_passcode_liveness(request, rule_id, to_gateway, to_id):
	rule_id = int(rule_id)
	to_id = int(to_id)
	_, to_principal, _, _ = get_gateway_and_principal_helper(to_gateway, to_id)

	try:
		auth_instance = PasscodeAuthInstance.objects.get(
			rule__id=rule_id, principal=to_principal)
	except PasscodeAuthInstance.DoesNotExist:
		return HttpResponse(status=403)	# denied

	if timezone.now() > auth_instance.expire:
		return HttpResponse(status=403)	# denied
	else:
		return HttpResponse(auth_instance.expire.strftime("%s"), status=200)

#-------------------------------------------------------------------------------