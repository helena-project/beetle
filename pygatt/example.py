import sys
import socket
import argparse

import lib.gatt as gatt
import lib.att as att
import lib.uuid as uuid

from framework import ManagedSocket
from server import GattServer, ServerError
from client import GattClient, ClientError

def onDisconnect(err):
	raise err

managedSocket = ManagedSocket()
server = GattServer(managedSocket, onDisconnect=onDisconnect)
client = GattClient(managedSocket)

# TODO