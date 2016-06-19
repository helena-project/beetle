"""
Beetle controller global application level constants defined here.
"""

NUM_HIGH_PRIORITY_LEVELS = 1

# Port used by gateways to listen for connections.
GATEWAY_TCP_SERVER_PORT = 3002

# Port to serve internal APIs.
CONTROLLER_REST_API_PORT = 3003

# Port for remote control server to listen on.
CONTROLLER_MANAGER_PORT = 3004

# Length of random session token assigned on gateway connection.
SESSION_TOKEN_LEN = 32

# Length of time in seconds to timeout disconnected gateways.
GATEWAY_CONNECTION_TIMEOUT = 60 * 5
