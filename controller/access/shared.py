from django.db.models import Q

from .models import Rule, RuleException

def _rule_is_static_subset(rule1, rule2):
	"""
	Returns whether a rule1 is a static subset of rule2.
	"""
	def _lte(a, b):
		return a == b or b.name == "*"

	is_static_subset = True
	is_static_subset &= _lte(rule1.from_principal, rule2.from_principal)
	is_static_subset &= _lte(rule1.from_gateway, rule2.from_gateway)
	is_static_subset &= _lte(rule1.to_principal, rule2.to_principal)
	is_static_subset &= _lte(rule1.to_gateway, rule2.to_gateway)
	is_static_subset &= _lte(rule1.service, rule2.service)
	is_static_subset &= _lte(rule1.characteristic, rule2.characteristic)
	is_static_subset &= rule1.cron_expression == rule2.cron_expression
	
	is_access_subset = True
	is_access_subset &= set(rule1.properties) == set(rule2.properties)
	is_access_subset &= rule1.exclusive == rule2.exclusive
	is_access_subset &= rule1.integrity == rule2.integrity
	is_access_subset &= rule1.encryption == rule2.encryption
	is_access_subset &= rule1.lease_duration == rule2.lease_duration

	return is_static_subset, is_access_subset

def _get_applicable_rules(from_gateway, from_principal, to_gateway, 
	to_principal, timestamp=None):
	"""
	Get the list of rules that apply for the mapping.
	"""
	rules = Rule.objects.filter(
		Q(from_principal=from_principal) | Q(from_principal__name="*"),
		Q(from_gateway=from_gateway) | Q(from_gateway__name="*"),
		Q(to_principal=to_principal) | Q(to_principal__name="*"),
		Q(to_gateway=to_gateway) | Q(to_gateway__name="*"),
		active=True)
	rules = rules.filter(start__lte=timestamp, expire__gte=timestamp) \
		| rules.filter(expire__isnull=True)
	return rules

def _is_rule_exception(rule, from_gateway, from_principal, to_gateway, 
	to_principal):
	"""
	Return whether the mapping is exempted from the rule.
	"""
	return RuleException.objects.filter(
		Q(from_principal=from_principal) | Q(from_principal__name="*"),
		Q(from_gateway=from_gateway) | Q(from_gateway__name="*"),
		Q(to_principal=to_principal) | Q(to_principal__name="*"),
		Q(to_gateway=to_gateway) | Q(to_gateway__name="*"),
		rule=rule).exists()

def query_can_map_static(from_gateway, from_principal, to_gateway, to_principal, 
	timestamp):
	"""
	Returns all of the static rules that allow a mapping.
	"""
	rules = _get_applicable_rules(from_gateway, from_principal, 
		to_gateway, to_principal, timestamp)
	
	for rule in rules:
		if _is_rule_exception(rule, from_gateway, from_principal, to_gateway, 
			to_principal):
			rules = rules.exclude(rule)
	return rules.exists(), rules
