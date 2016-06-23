

from django.shortcuts import render_to_response
from django.http import HttpResponse
from django.db import transaction
from django.db.models import Q
from django.utils import timezone
from django.views.decorators.http import require_http_methods
from django.template import RequestContext

from passlib.apps import django_context as pwd_context

from beetle.models import Principal
from access.models import Rule, RuleException, DynamicAuth, PasscodeAuth

def __is_rule_exempt_helper(rule, principal):
	return bool(RuleException.objects.filter(
		Q(to_principal=principal) | Q(to_principal__name="*"),
		rule=rule))

@require_http_methods(["GET", "POST"])
@transaction.atomic
def view_form_passcode(request, rule, principal):
	"""Serve the password form and receive responses."""
	context = RequestContext(request)

	try:
		rule = Rule.objects.get(name=rule)
		principal = Principal.objects.get(name=principal)
		passcode_auth = DynamicAuth.objects.instance_of(PasscodeAuth).get(
			rule=rule)
	except DynamicAuth.DoesNotExist:
		context_dict = {
			"title" : "An error occurred...",
			"message" : "%s does not require passcode auth." % (rule.name,),
		}
		return render_to_response('state/empty_response.html',
			context_dict, context)
	except Exception, err:
		context_dict = {
			"title" : "An error occurred...",
			"message" : str(err),
		}
		return render_to_response('state/empty_response.html',
			context_dict, context)

	now = timezone.now()

	context = RequestContext(request)
	context_dict = {
		"rule" : rule,
		"principal" : principal,
		"auth" : passcode_auth
	}

	if (rule.to_principal.name != "*" and rule.to_principal != principal) or \
		__is_rule_exempt_helper(rule, principal):
		#######################
		# Rule does not apply #
		#######################
		context_dict.update({
			"status" : {
				"a" : "Does Not Apply",
				"b" : "denied",
			}
		})
		return render_to_response('state/passcode_form_submit.html',
			context_dict, context)

	if request.method == "GET":
		#######################
		# Requesting the form #
		#######################
		if passcode_auth.code == "":
			# No passcode, grant access
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
			return render_to_response('state/passcode_form_submit.html',
				context_dict, context)
		else:
			# Render the password form
			return render_to_response('state/passcode_form.html',
				context_dict, context)

	elif request.method == "POST":
		#######################
		# Submitting the form #
		#######################
		code = request.POST["code"]
		allowed = False
		try:
			allowed = pwd_context.verify(code, passcode_auth.chash)
		except Exception:
			pass

		if not allowed:
			context_dict.update({
				"status" : {
					"a" : "Invalid code",
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

		return render_to_response('state/passcode_form_submit.html',
			context_dict, context)

	else:
		return HttpResponse(status=403)

@require_http_methods(["GET","POST"])
@transaction.atomic
def view_form_passcode_generic(request, rule):
	"""Serve the password form and receive responses."""
	context = RequestContext(request)

	try:
		rule = Rule.objects.get(name=rule)
		passcode_auth = DynamicAuth.objects.instance_of(PasscodeAuth).get(
			rule=rule)
	except DynamicAuth.DoesNotExist:
		context_dict = {
			"title" : "An error occurred...",
			"message" : "%s does not require passcode auth." % (rule.name,),
		}
		return render_to_response('state/empty_response.html',
			context_dict, context)
	except Exception, err:
		context_dict = {
			"title" : "An error occurred...",
			"message" : str(err),
		}
		return render_to_response('state/empty_response.html',
			context_dict, context)

	now = timezone.now()

	context = RequestContext(request)
	context_dict = {
		"rule" : rule,
		"auth" : passcode_auth
	}

	if request.method == "GET":
		#######################
		# Requesting the form #
		#######################

		# get list of principals that the rule can be applied to
		if rule.to_principal.name == "*":
			principals = Principal.objects.all().exclude(name="*")
		else:
			principals = [rule.to_principal]
		principals = [x for x in principals if not __is_rule_exempt_helper(
			rule, x)]

		context_dict["principals"] = principals

		return render_to_response('state/passcode_form_generic.html',
			context_dict, context)

	elif request.method == "POST":
		#######################
		# Submitting the form #
		#######################
		code = request.POST["code"]
		principal = request.POST["principal"]
		principal = Principal.objects.get(name=principal)

		if (rule.to_principal.name != "*" and rule.to_principal != principal) \
			or __is_rule_exempt_helper(rule, principal):
			context_dict = {
				"title" : "An error occurred...",
				"message" : "%s does not apply for %s." % (rule.name,
					principal.name),
			}
			return render_to_response('state/empty_response.html',
				context_dict, context)

		context_dict["principal"] = principal

		allowed = False
		try:
			if passcode_auth.code != "":
				allowed = pwd_context.verify(code, passcode_auth.chash)
			elif code == "":
				allowed = True
		except Exception:
			pass

		if not allowed:
			context_dict.update({
				"status" : {
					"a" : "Invalid code",
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
		return render_to_response('state/passcode_form_submit.html',
			context_dict, context)

	else:
		return HttpResponse(status=403)
