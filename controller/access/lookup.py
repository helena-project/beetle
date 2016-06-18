from django.db.models import Q

from beetle.models import PrincipalGroup, GatewayGroup

from .models import Rule, RuleException

def __get_applicable_rules(from_gateway, from_principal, to_gateway,
	to_principal, timestamp=None):
	"""Get the queryset of rules that apply for the mapping."""

	to_principal_groups = PrincipalGroup.objects.filter(
		members__in=[to_principal])
	from_principal_groups = PrincipalGroup.objects.filter(
		members__in=[from_principal])

	to_gateway_groups = GatewayGroup.objects.filter(
		members__in=[to_gateway])
	from_gateway_groups = GatewayGroup.objects.filter(
		members__in=[from_gateway])

	rules = Rule.objects.filter(
		Q(from_principal=from_principal) | Q(from_principal__name="*") |
			Q(from_principal__in=from_principal_groups),
		Q(from_gateway=from_gateway) | Q(from_gateway__name="*") |
			Q(from_gateway__in=from_gateway_groups),
		Q(to_principal=to_principal) | Q(to_principal__name="*") |
			Q(to_principal__in=to_principal_groups),
		Q(to_gateway=to_gateway) | Q(to_gateway__name="*") |
			Q(to_gateway__in=to_gateway_groups),
		active=True)

	rules = rules.filter(start__lte=timestamp, expire__gte=timestamp) \
		| rules.filter(expire__isnull=True)

	return rules

def __get_rule_exceptions(rules, from_gateway, from_principal, to_gateway,
	to_principal):
	"""Get the queryset of rule ids that should be excluded."""

	to_principal_groups = PrincipalGroup.objects.filter(
		members__in=[to_principal])
	from_principal_groups = PrincipalGroup.objects.filter(
		members__in=[from_principal])

	to_gateway_groups = GatewayGroup.objects.filter(
		members__in=[to_gateway])
	from_gateway_groups = GatewayGroup.objects.filter(
		members__in=[from_gateway])

	return RuleException.objects.filter(
		Q(from_principal=from_principal) | Q(from_principal__name="*") |
			Q(from_principal__in=from_principal_groups),
		Q(from_gateway=from_gateway) | Q(from_gateway__name="*") |
			Q(from_gateway__in=from_gateway_groups),
		Q(to_principal=to_principal) | Q(to_principal__name="*") |
			Q(to_principal__in=to_principal_groups),
		Q(to_gateway=to_gateway) | Q(to_gateway__name="*") |
			Q(to_gateway__in=to_gateway_groups),
		rule__in=rules).values_list("rule_id", flat=True).distinct()

def query_can_map_static(from_gateway, from_principal, to_gateway, to_principal,
	timestamp):
	"""Returns all of the static rules that allow a mapping."""

	rules = __get_applicable_rules(from_gateway, from_principal,
		to_gateway, to_principal, timestamp)

	exceptions = __get_rule_exceptions(rules, from_gateway, from_principal,
		to_gateway, to_principal)

	rules = rules.exclude(id__in=exceptions)

	return rules.exists(), rules
