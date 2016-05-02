from django.db.models import Q

from .models import Rule, RuleException

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
			rules = rules.exclude(id=rule.id)
	return rules.exists(), rules
