
from django.core.management.base import BaseCommand

from main.constants import CONTROLLER_MANAGER_PORT

import manager.application.manager as manager

class Command(BaseCommand):

	def add_arguments(self, parser):
		parser.add_argument(
			'--port',
			type=int,
			default=CONTROLLER_MANAGER_PORT,
			help="Port for control server to listen on.",
		)
		parser.add_argument(
			'--host',
			type=str,
			default="",
			help="Interface to listen on.",
		)

	def handle(self, *args, **options):
		"""Start and run the server"""

		host = options['host']
		port = options['port']

		manager.run(host, port)
