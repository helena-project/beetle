# Beetle for Linux

##### Requirements
- libboost-all-dev, libbluetooth-dev, libcppnetlib-dev, libasio-dev, openssl
- Eclipse CDT (Mars)
- For Ubuntu 15.04, libcppnetlib needs to be compiled and installed manually: https://github.com/cpp-netlib/cpp-netlib.git

##### To build and run
1. clone and import the project into Eclipse
2. build
3. in a terminal, ``` cd Debug ```
4. ``` sudo ./Beetle --no-controller``` 

##### Python tcp client
In ```client``` subdirectory. 

##### Python controller
In ```controller``` subdirectory. Needed to run with the controller enabled.

##### TCP connection protocol
Parameters are key value pairs sent at the beginning of the connection in ASCII text. 

* ```client name``` *(connect with the client protocol, where name is the name of the client)*
* ```server bool``` *(where bool is whether the client is running a GATT server)*
* ```gateway name``` *(connect with the gateway protocol, where name is the name of the gateway)*
* ```device id``` *(where id is the id of the device at the gateway)*