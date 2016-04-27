from .models import ConnectedGateway, ConnectedPrincipal

from beetle.models import Gateway, Principal

def get_gateway_and_principal_helper(gateway, remote_id):
	"""
	Gateway is a string, remote_id is an int.
	"""
	gateway = Gateway.objects.get(name=gateway)
	conn_gateway = ConnectedGateway.objects.get(gateway=gateway)
	conn_principal = ConnectedPrincipal.objects.get(gateway=conn_gateway, 
		remote_id=remote_id)
	principal = conn_principal.principal
	return gateway, principal, conn_gateway, conn_principal

def get_gateway_helper(gateway):
	"""
	Same as above without an principal.
	"""
	gateway = Gateway.objects.get(name=gateway)
	conn_gateway = ConnectedGateway.objects.get(gateway=gateway)
	return gateway, conn_gateway
