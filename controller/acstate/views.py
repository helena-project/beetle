from django.shortcuts import render, render_to_response
from django.http import JsonResponse, HttpResponse
from django.db import transaction
from django.db.models import Q
from django.core import serializers
from django.utils import timezone
from django.views.decorators.http import require_GET, require_POST, \
	require_http_methods
from django.template import RequestContext
from django.views.decorators.csrf import csrf_exempt

from passlib.apps import django_context as pwd_context

from .models import AdminAuthInstance, UserAuthInstance, \
	PasscodeAuthInstance, BeetleEmailAccount, ExclusiveLease

from beetle.models import Principal, Gateway, Contact
from gatt.models import Service, Characteristic
from access.models import Rule, RuleException, DynamicAuth, PasscodeAuth, \
	AdminAuth, UserAuth, NetworkAuth, Exclusive
from network.models import ConnectedGateway, ConnectedPrincipal, \
	ServiceInstance, CharInstance
from network.shared import get_gateway_and_principal_helper, get_gateway_helper

import dateutil.parser
from dateutil.relativedelta import relativedelta
import cronex
import json
import base64
import random
import string
import time
import re

import smtplib
import imaplib
from email.MIMEMultipart import MIMEMultipart
from email.MIMEText import MIMEText

#------------------------------------------------------------------------------

def _is_rule_exempt_helper(rule, principal):
	return bool(RuleException.objects.filter(
		Q(to_principal=principal) | Q(to_principal__name="*"),
		rule=rule))

#------------------------------------------------------------------------------

@require_http_methods(["GET","POST"])
@transaction.atomic
def view_form_passcode(request, rule, principal):
	"""
	Serve the password form and receive responses.
	"""
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
		return render_to_response('authstate/empty_response.html', 
			context_dict, context)
	except Exception, err:
		context_dict = {
			"title" : "An error occurred...",
			"message" : str(err),
		}
		return render_to_response('authstate/empty_response.html', 
			context_dict, context)

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
			return render_to_response('authstate/passcode_form_submit.html', 
				context_dict, context)
		else:
			# Render the password form
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
		return render_to_response('authstate/passcode_form_submit.html', 
			context_dict, context)
	else:
		return HttpResponse(status=403)

@require_http_methods(["GET","POST"])
@transaction.atomic
def view_form_passcode_generic(request, rule):
	"""
	Serve the password form and receive responses.
	"""
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
		return render_to_response('authstate/empty_response.html', 
			context_dict, context)
	except Exception, err:
		context_dict = {
			"title" : "An error occurred...",
			"message" : str(err),
		}
		return render_to_response('authstate/empty_response.html', 
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
		principals = [x for x in principals if not _is_rule_exempt_helper(
			rule, x)]

		context_dict["principals"] = principals

		return render_to_response('authstate/passcode_form_generic.html', 
			context_dict, context)
	elif request.method == "POST":
		#######################
		# Submitting the form #
		#######################
		code = request.POST["code"]
		principal = request.POST["principal"]
		principal = Principal.objects.get(name=principal)

		if (rule.to_principal.name != "*" and rule.to_principal != principal) \
			or _is_rule_exempt_helper(rule, principal):
			context_dict = {
				"title" : "An error occurred...",
				"message" : "%s does not apply for %s." % (rule.name, 
					principal.name),
			}
			return render_to_response('authstate/empty_response.html', 
				context_dict, context)

		context_dict["principal"] = principal

		allowed = False
		try:
			if passcode_auth.code != "":
				allowed = pwd_context.verify(code, passcode_auth.chash) 
			elif code == "":
				allowed = True
		except:
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
		return render_to_response('authstate/passcode_form_submit.html', 
			context_dict, context)
	else:
		return HttpResponse(status=403)


@require_GET
def query_passcode_liveness(request, rule_id, to_gateway, to_id):
	"""
	Query for whether the password has been added.
	"""
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

#------------------------------------------------------------------------------

ATT = "att"
TMOBILE = "tmobile"
VERIZON = "verizon"
SPRINT = "sprint"

SMS_GATEWAYS = {
	ATT : "txt.att.net",
	TMOBILE : "tmomail.net",
	VERIZON : "vtext.com",
	SPRINT : "page.nextel.com",
}

EMAIL_REPLY_REGEX = re.compile(
	r"X-OPWV-Extra-Message-Type:Reply\s+(\w+?)\s+-----Original Message-----")

def _generate_rand_string(otp_len=6):
	return ''.join(random.choice(string.ascii_uppercase + string.digits) 
		for _ in range(otp_len))

#------------------------------------------------------------------------------

def _format_admin_auth_body(rule, principal, auth):
	msg = "'%s' is requesting access for rule '%s'." % (principal.name, 
		rule.name)
	if auth.message:
		msg += " %s." % (auth.message,) 
	msg += " Text OK to approve."
	return msg

@csrf_exempt
@require_POST
def request_admin_auth(request, rule_id, to_gateway, to_id):
	"""
	Request for admin authorization
	"""
	rule_id = int(rule_id)
	to_id = int(to_id)
	current_time = timezone.now()

	rule = Rule.objects.get(id=rule_id)
	_, to_principal, _, _ = get_gateway_and_principal_helper(to_gateway, to_id)

	try:
		admin_auth = DynamicAuth.objects.instance_of(AdminAuth).get(rule=rule)
	except DynamicAuth.DoesNotExist:
		return HttpResponse("rule has no admin auth", status=400)

	# check if already authorized
	try:
		auth_instance = AdminAuthInstance.objects.get(
			rule=rule, principal=to_principal)
		if auth_instance is not None:
			if auth_instance.expire >= current_time:
				######################
				# Already authorized #
				######################
				return HttpResponse(auth_instance.expire.strftime("%s"), 
					status=202)
			else:
				auth_instance.delete() 
	except AdminAuthInstance.DoesNotExist:
		pass

	###########################################
	# not yet authorized, send a text message #
	###########################################
	controller_email_acct = BeetleEmailAccount.objects.get()
	if not controller_email_acct.address:
		return HttpResponse("no sender address set up at server", status=500)

	# TODO fix this to not be AT&T
	email = admin_auth.admin.phone_number.replace("-","") + "@" \
		+ SMS_GATEWAYS[ATT]
	email = email.encode('ascii','ignore')

	server = smtplib.SMTP('smtp.gmail.com', 587)
	server.starttls()
	server.login(controller_email_acct.address, controller_email_acct.password)

	# write the sms
	msg_id = _generate_rand_string()
	subject = "Admin Auth (%s)" % (msg_id,)
	body = _format_admin_auth_body(rule, to_principal, admin_auth)
	
	msg = MIMEMultipart()
	msg['From'] = controller_email_acct.address
	msg['To'] = admin_auth.admin.first_name
	msg['Subject'] = subject
	msg.attach(MIMEText(body, 'plain'))

	server.sendmail(controller_email_acct.address, email, msg.as_string())
	server.quit()

	# wait for response
	mail = imaplib.IMAP4_SSL('imap.gmail.com')
	mail.login(controller_email_acct.address, controller_email_acct.password)

	timeout = current_time + relativedelta(seconds=60)
	while timezone.now() < timeout:
		time.sleep(5)
		mail.select("inbox")
		result, data = mail.search(None, '(HEADER Subject "%s")' % (subject,))
		ids = data[0].split()

		if not ids:
			print "still waiting..."
			continue
		
		latest_email_id = ids[-1]
		result, data = mail.fetch(latest_email_id, "(RFC822)") 
		raw_email = data[0][1]
		
		email_match = EMAIL_REPLY_REGEX.search(raw_email)
		if email_match is not None:
			reply = email_match.group(1).lower()
			if reply == "ok":
				##############
				# Allow once #
				##############
				auth_instance = AdminAuthInstance(
					rule=rule, 
					principal=to_principal,
					expire=current_time + admin_auth.session_length)
				auth_instance.save()
				return HttpResponse(auth_instance.expire.strftime("%s"), 
					status=202)
			elif reply == "always":
				################
				# Allow always #
				################
				auth_instance = AdminAuthInstance(
					rule=rule, 
					principal=to_principal,
					expire=current_time + relativedelta(years=1))
				auth_instance.save()
				return HttpResponse(auth_instance.expire.strftime("%s"), 
					status=202)
			elif reply == "never":
				###############
				# Deny always #
				###############
				wildcard_gateway = Gateway.objects.get(name="*")
				wildcard_principal = Principal.objects.get(name="*")
				rule_exception, created = RuleException.objects.get_or_create(
					rule=rule,
					from_gateway=wildcard_gateway,
					from_principal=wildcard_principal,
					to_principal=to_principal,
					to_gateway=wildcard_gateway)
				rule_exception.save()
				return HttpResponse("denied permanently", status=403)
		else:
			print "no match"

	return HttpResponse("no response from admin", status=408)

#------------------------------------------------------------------------------

def _format_user_auth_body(rule, principal, auth):
	msg = "Confirm access for '%s' to rule '%s'." % (principal.name, rule.name)
	if auth.message:
		msg += " %s." % (auth.message,) 
	msg += " Text OK to approve."
	return msg

@csrf_exempt
@require_POST
def request_user_auth(request, rule_id, to_gateway, to_id):
	"""
	Request for user authenication
	"""
	rule_id = int(rule_id)
	to_id = int(to_id)
	current_time = timezone.now()

	rule = Rule.objects.get(id=rule_id)
	_, to_principal, _, _ = get_gateway_and_principal_helper(to_gateway, to_id)

	try:
		user_auth = DynamicAuth.objects.instance_of(UserAuth).get(rule=rule)
	except DynamicAuth.DoesNotExist:
		return HttpResponse("rule has no user auth", status=400)

	# check if already authorized
	try:
		auth_instance = UserAuthInstance.objects.get(
			rule=rule, principal=to_principal)
		if auth_instance is not None:
			if auth_instance.expire >= current_time:
				######################
				# Already authorized #
				######################
				return HttpResponse(auth_instance.expire.strftime("%s"), 
					status=202)
			else:
				auth_instance.delete() 
	except UserAuthInstance.DoesNotExist:
		pass

	###########################################
	# not yet authorized, send a text message #
	###########################################
	controller_email_acct = BeetleEmailAccount.objects.get()
	if not controller_email_acct.address:
		return HttpResponse("no sender address set up at server", status=500)

	# TODO fix this to not be AT&T
	email = to_principal.owner.phone_number.replace("-","") + "@" \
		+ SMS_GATEWAYS[ATT]
	email = email.encode('ascii','ignore')

	server = smtplib.SMTP('smtp.gmail.com', 587)
	server.starttls()
	server.login(controller_email_acct.address, controller_email_acct.password)

	# write the sms
	msg_id = _generate_rand_string()
	subject = "User Auth (%s)" % (msg_id,)
	body = _format_user_auth_body(rule, to_principal, user_auth)
	
	msg = MIMEMultipart()
	msg['From'] = controller_email_acct.address
	msg['To'] = to_principal.owner.first_name
	msg['Subject'] = subject
	msg.attach(MIMEText(body, 'plain'))

	server.sendmail(controller_email_acct.address, email, msg.as_string())
	server.quit()

	# wait for response
	mail = imaplib.IMAP4_SSL('imap.gmail.com')
	mail.login(controller_email_acct.address, controller_email_acct.password)

	timeout = current_time + relativedelta(seconds=60)
	while timezone.now() < timeout:
		time.sleep(5)
		mail.select("inbox")
		result, data = mail.search(None, '(HEADER Subject "%s")' % (subject,))
		ids = data[0].split()

		if not ids:
			print "still waiting..."
			continue
		
		latest_email_id = ids[-1]
		result, data = mail.fetch(latest_email_id, "(RFC822)") 
		raw_email = data[0][1]
		
		email_match = EMAIL_REPLY_REGEX.search(raw_email)
		if email_match is not None:
			reply = email_match.group(1).lower()
			if reply == "ok":
				##############
				# Allow once #
				##############
				auth_instance = UserAuthInstance(
					rule=rule, 
					principal=to_principal,
					expire=current_time + user_auth.session_length)
				auth_instance.save()
				return HttpResponse(auth_instance.expire.strftime("%s"), 
					status=202)
			elif reply == "never":
				###############
				# Deny always #
				###############
				wildcard_gateway = Gateway.objects.get(name="*")
				wildcard_principal = Principal.objects.get(name="*")
				rule_exception, created = RuleException.objects.get_or_create(
					rule=rule,
					from_gateway=wildcard_gateway,
					from_principal=wildcard_principal,
					to_principal=to_principal,
					to_gateway=wildcard_gateway)
				rule_exception.save()
				return HttpResponse("denied permanently", status=403)
		else:
			print "no match"


	return HttpResponse("no response from user", status=408)

#------------------------------------------------------------------------------

@csrf_exempt
@require_http_methods(["POST", "DELETE"])
@transaction.atomic
def request_exclusive_lease(request, exclusive_id, to_gateway, to_id):
	"""
	Acquire or release a lease on an exclusive group.
	"""
	exclusive_id = int(exclusive_id)
	to_id = int(to_id)
	current_time = timezone.now()

	_, _, _, to_principal_conn = get_gateway_and_principal_helper(to_gateway, to_id)

	exclusive = Exclusive.objects.get(id=exclusive_id)

	if request.method == "POST":
		###########
		# Acquire #
		###########
		success = False
		try:
			exclusive_lease = ExclusiveLease.objects.get(group=exclusive)
			if current_time > exclusive_lease.expire:
				exclusive_lease.expire = current_time + exclusive.default_lease
				exclusive_lease.principal = to_principal_conn
				exclusive_lease.timestamp = current_time
				exclusive_lease.save()
				success = True
			elif exclusive_lease.principal == to_principal_conn:
				success = True
		except ExclusiveLease.DoesNotExist:
			exclusive_lease = ExclusiveLease(
				group=exclusive,
				principal=to_principal_conn,
				expire=current_time + exclusive.default_lease)
			exclusive_lease.save()
			success = True

		if success:
			return HttpResponse(exclusive_lease.expire.strftime("%s"), status=202)
		else:
			return HttpResponse("denied", status=403)

	elif request.method == "DELETE":
		###########
		# Release #
		###########
		try:
			exclusive_lease = ExclusiveLease.objects.get(group=exclusive)
			if exclusive_lease.principal == to_principal_conn:
				exclusive_lease.delete()
		except ExclusiveLease.DoesNotExist:
			pass
		return HttpResponse(status=200)

	else:
		return HttpResponse(status=403)

