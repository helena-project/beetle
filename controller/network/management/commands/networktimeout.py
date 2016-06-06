
from datetime import timedelta

from django.core.management.base import BaseCommand
from django.utils import timezone

from main.constants import GATEWAY_CONNECTION_TIMEOUT

from network.models import ConnectedGateway

class Command(BaseCommand):

	def add_arguments(self, parser):
		pass

	def handle(self, *args, **options):
		"""Delete connected gateways that have timed out"""

		threshold = timezone.now() - timedelta(
			seconds=GATEWAY_CONNECTION_TIMEOUT)
		qs = ConnectedGateway.objects.filter(is_connected=False, 
			last_updated__lt=threshold)
		qs.delete()
