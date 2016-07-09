
import random
import string
import time
import re

import smtplib
import imaplib
from email.MIMEMultipart import MIMEMultipart
from email.MIMEText import MIMEText

from dateutil.relativedelta import relativedelta

from django.http import HttpResponse
from django.db import transaction
from django.utils import timezone
from django.views.decorators.http import require_GET, require_POST, \
	require_http_methods
from django.views.decorators.csrf import csrf_exempt

from .models import AdminAuthInstance, UserAuthInstance, \
	PasscodeAuthInstance, ExclusiveLease

from beetle.models import BeetleEmailAccount, Contact
from access.models import Rule, DynamicAuth, AdminAuth, UserAuth, Exclusive
from network.lookup import get_gateway_and_device_helper

from utils.decorators import require_api_port

@require_api_port
@require_GET
def query_passcode_liveness(request, rule_id, to_gateway, to_id):
	"""Query for whether the password has been added."""

	rule_id = int(rule_id)
	to_id = int(to_id)
	_, to_principal, _, _ = get_gateway_and_device_helper(to_gateway, to_id)

	try:
		auth_instance = PasscodeAuthInstance.objects.get(
			rule__id=rule_id, principal=to_principal)
	except PasscodeAuthInstance.DoesNotExist:
		return HttpResponse(status=403)	# denied

	if timezone.now() > auth_instance.expire:
		return HttpResponse(status=403)	# denied
	else:
		return HttpResponse(auth_instance.expire.strftime("%s"), status=200)

# TODO(James): each carrier does their own wonky thing...
# EMAIL_REPLY_REGEX = re.compile(
# 	r"X-OPWV-Extra-Message-Type:Reply\s+(\w+?)\s+*")
EMAIL_REPLY_REGEX = re.compile(
	r".*\s+(?P<action>(OK)|(ALWAYS)|(NEVER))\s+.*", re.IGNORECASE)

def __generate_rand_string(otp_len=6):
	return ''.join(random.choice(string.ascii_uppercase + string.digits)
		for _ in range(otp_len))

def __format_admin_auth_body(rule, principal, auth):
	msg = "'%s' is requesting access for rule '%s'." % (principal.name,
		rule.name)
	if auth.message:
		msg += " %s." % (auth.message,)
	msg += " Text OK to approve."
	return msg

@csrf_exempt
@require_api_port
@require_POST
def request_admin_auth(request, rule_id, to_gateway, to_id):
	"""Request for admin authorization"""

	rule_id = int(rule_id)
	to_id = int(to_id)
	current_time = timezone.now()

	rule = Rule.objects.get(id=rule_id)
	_, to_principal, _, _ = get_gateway_and_device_helper(to_gateway, to_id)

	try:
		admin_auth = DynamicAuth.objects.instance_of(AdminAuth).get(rule=rule)
	except DynamicAuth.DoesNotExist:
		return HttpResponse("rule has no admin auth", status=400)

	# check if already authorized
	try:
		auth_instance = AdminAuthInstance.objects.get(
			rule=rule, principal=to_principal)
		if auth_instance is not None:
			if auth_instance.allow:
				if auth_instance.expire >= current_time:
					######################
					# Already authorized #
					######################
					return HttpResponse(auth_instance.expire.strftime("%s"),
						status=202)
				else:
					auth_instance.delete()
			else:
				return HttpResponse("denied permanently", status=403)
	except AdminAuthInstance.DoesNotExist:
		pass

	###########################################
	# not yet authorized, send a text message #
	###########################################
	controller_email_acct = BeetleEmailAccount.objects.get()
	if not controller_email_acct.address:
		return HttpResponse("no sender address set up at server", status=500)

	if to_principal.owner.auth_method == Contact.AUTH_EMAIL:
		email = admin_auth.admin.email_address
	else:
		email = admin_auth.admin.phone_number.replace("-","") + "@" \
			+ admin_auth.admin.carrier_gateway
	email = email.encode('ascii','ignore')

	server = smtplib.SMTP('smtp.gmail.com', 587)
	server.starttls()
	server.login(controller_email_acct.address, controller_email_acct.password)

	# write the sms
	msg_id = __generate_rand_string()
	subject = "Admin Auth (%s)" % (msg_id,)
	body = __format_admin_auth_body(rule, to_principal, admin_auth)

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

	timeout = current_time + relativedelta(seconds=120)
	while timezone.now() < timeout:
		time.sleep(5)
		mail.select("inbox")
		_, data = mail.search(None, '(HEADER Subject "%s")' % (subject,))
		ids = data[0].split()

		if not ids:
			print "still waiting..."
			continue

		latest_email_id = ids[-1]
		_, data = mail.fetch(latest_email_id, "(RFC822)")
		raw_email = data[0][1]

		email_match = EMAIL_REPLY_REGEX.search(raw_email)
		if email_match is not None:
			reply = email_match.groupdict()['action'].lower()
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
				auth_instance = AdminAuthInstance(
					rule=rule,
					allow=False,
					principal=to_principal,
					expire=current_time + relativedelta(years=100))
				auth_instance.save()
				return HttpResponse("denied permanently", status=403)
		else:
			print "no match"

	return HttpResponse("no response from admin", status=408)

def __format_user_auth_body(rule, principal, auth):
	msg = "Confirm access for '%s' to rule '%s'." % (principal.name, rule.name)
	if auth.message:
		msg += " %s." % (auth.message,)
	msg += " Text OK to approve."
	return msg

@csrf_exempt
@require_api_port
@require_POST
def request_user_auth(request, rule_id, to_gateway, to_id):
	"""Request for user authentication"""

	rule_id = int(rule_id)
	to_id = int(to_id)
	current_time = timezone.now()

	rule = Rule.objects.get(id=rule_id)
	_, to_principal, _, _ = get_gateway_and_device_helper(to_gateway, to_id)

	try:
		user_auth = DynamicAuth.objects.instance_of(UserAuth).get(rule=rule)
	except DynamicAuth.DoesNotExist:
		return HttpResponse("rule has no user auth", status=400)

	# check if already authorized
	try:
		auth_instance = UserAuthInstance.objects.get(
			rule=rule, principal=to_principal)
		if auth_instance is not None:
			if auth_instance.allow:
				if auth_instance.expire >= current_time:
					######################
					# Already authorized #
					######################
					return HttpResponse(auth_instance.expire.strftime("%s"),
						status=202)
				else:
					auth_instance.delete()
			else:
				return HttpResponse("denied permanently", status=403)
	except UserAuthInstance.DoesNotExist:
		pass

	###########################################
	# not yet authorized, send a text message #
	###########################################
	controller_email_acct = BeetleEmailAccount.objects.get()
	if not controller_email_acct.address:
		return HttpResponse("no sender address set up at server", status=500)

	if to_principal.owner.auth_method == Contact.AUTH_EMAIL:
		email = to_principal.owner.email_address
	else:
		email = to_principal.owner.phone_number.replace("-", "") + "@" \
			+ to_principal.owner.carrier_gateway
	email = email.encode('ascii', 'ignore')

	server = smtplib.SMTP('smtp.gmail.com', 587)
	server.starttls()
	server.login(controller_email_acct.address, controller_email_acct.password)

	# write the sms
	msg_id = __generate_rand_string()
	subject = "User Auth (%s)" % (msg_id,)
	body = __format_user_auth_body(rule, to_principal, user_auth)

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
		_, data = mail.search(None, '(HEADER Subject "%s")' % (subject,))
		ids = data[0].split()

		if not ids:
			print "still waiting..."
			continue

		latest_email_id = ids[-1]
		_, data = mail.fetch(latest_email_id, "(RFC822)")
		raw_email = data[0][1]

		email_match = EMAIL_REPLY_REGEX.search(raw_email)
		if email_match is not None:
			reply = email_match.groupdict()['action'].lower()
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
				auth_instance = UserAuthInstance(
					rule=rule,
					allow=False,
					principal=to_principal,
					expire=current_time + relativedelta(years=100))
				auth_instance.save()
				return HttpResponse("denied permanently", status=403)
		else:
			print "no match"

	return HttpResponse("no response from user", status=408)

@csrf_exempt
@require_api_port
@require_http_methods(["POST", "DELETE"])
@transaction.atomic
def request_exclusive_lease(request, exclusive_id, to_gateway, to_id):
	"""Acquire or release a lease on an exclusive group."""

	exclusive_id = int(exclusive_id)
	to_id = int(to_id)
	current_time = timezone.now()

	_, _, _, to_principal_conn = get_gateway_and_device_helper(to_gateway, to_id)

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

