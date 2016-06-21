# Beetle Controller

Runs a HTTPS server in django to serve API and TCP server to push commands.

## Easy Setup

1. ```pip install -r requirements.txt```
2. ```./setup.sh```
3. ```sudo ./runserver.sh```
4. Navigate to the web interface on port 80 or 443 and add your gateways and
rules.

## Requirements

- redis-server

## Notes

```runserver.sh``` starts a development version of the server.

## Certs

This contains default certificates for testing.