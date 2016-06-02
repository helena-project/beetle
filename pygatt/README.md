# GATT library in Python

Existing BLE/GATT libraries are bound to operating system APIs; the Bluetooth
IC and hardware; or do not support simultaneous GATT client and server modes. 
```pygatt``` implements the GATT protocol as a standalone library above the 
transport layer. It supports simultaneous client/server modes; is agnostic 
to the OS, the underlying link and the network.

## ManagedSocket
This wraps around a regular socket, either with seqpackets or a stream. 

## GattClient
This is bound to a ManagedSocket and allows the application to discover 
services on the Beetle gateway.

## GattServer
This is bound to a ManagedSocket and allows the application to serve GATT 
services, characteristics, and descriptors.

## Usage
See ```example.py``` for an implementation of a heart rate monitor that also 
acts as a GATT client.

## Notes
This library is a work in progress. Read, notify, indicate are tested and 
working. See the example application for details.