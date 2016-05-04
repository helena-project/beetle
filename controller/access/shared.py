from django.db.models import Q

from beetle.models import Principal

from .models import Rule, RuleException

def _get_applicable_rules(from_gateway, from_principal, to_gateway, 
	to_principal, timestamp=None):
	"""
	Get the queryset of rules that apply for the mapping.
	"""
	rules = Rule.objects.filter(
		Q(from_principal=from_principal) | Q(from_principal__name="*") | 
			Q(from_principal__ptype=Principal.GROUP, 
				from_principal__members__in=[from_principal]),
		Q(from_gateway=from_gateway) | Q(from_gateway__name="*"), 
		Q(to_principal=to_principal) | Q(to_principal__name="*") | 
			Q(to_principal__ptype=Principal.GROUP, 
				to_principal__members__in=[to_principal]),
		Q(to_gateway=to_gateway) | Q(to_gateway__name="*"),
		active=True)
	rules = rules.filter(start__lte=timestamp, expire__gte=timestamp) \
		| rules.filter(expire__isnull=True)
	return rules

def _get_rule_exceptions(rules, from_gateway, from_principal, to_gateway, 
	to_principal):
	"""
	Get the queryset of rule ids that should be excluded.
	"""
	return RuleException.objects.filter(
		Q(from_principal=from_principal) | Q(from_principal__name="*") | 
			Q(from_principal__ptype=Principal.GROUP, 
				from_principal__members__in=[from_principal]),
		Q(from_gateway=from_gateway) | Q(from_gateway__name="*"), 
		Q(to_principal=to_principal) | Q(to_principal__name="*") | 
			Q(to_principal__ptype=Principal.GROUP, 
				to_principal__members__in=[to_principal]),
		Q(to_gateway=to_gateway) | Q(to_gateway__name="*"),
		rule__in=rules).values_list("rule_id", flat=True).distinct()

def query_can_map_static(from_gateway, from_principal, to_gateway, to_principal, 
	timestamp):
	"""
	Returns all of the static rules that allow a mapping.
	"""
	rules = _get_applicable_rules(from_gateway, from_principal, 
		to_gateway, to_principal, timestamp)
	
	exceptions = _get_rule_exceptions(rules, from_gateway, from_principal, 
		to_gateway, to_principal)
	
	rules = rules.exclude(id__in=exceptions)
	
	return rules.exists(), rules
