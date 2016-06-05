from .models import ConnectedGateway, ConnectedDevice

from beetle.models import BeetleGateway

def get_gateway_and_device_helper(gateway, remote_id):
	"""Gateway is a string, remote_id is an int."""

	assert type(gateway) is str or type(gateway) is unicode
	assert type(remote_id) is int

	gateway = BeetleGateway.objects.get(name=gateway)
	conn_gateway = ConnectedGateway.objects.get(gateway=gateway)
	conn_device = ConnectedDevice.objects.get(gateway_instance=conn_gateway, 
		remote_id=remote_id)
	device = conn_device.device

	return gateway, device, conn_gateway, conn_device

def get_gateway_helper(gateway):
	"""Same as above without an device."""

	assert type(gateway) is str or type(gateway) is unicode
	
	gateway = BeetleGateway.objects.get(name=gateway)
	conn_gateway = ConnectedGateway.objects.get(gateway=gateway)
	
	return gateway, conn_gateway
