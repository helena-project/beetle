# Beetle C++ for Linux

##### Requirements
- libboost-all-dev, libbluetoothdev
- Eclipse CDT (Mars)

##### To build and run
1. clone and import the project into Eclipse
2. build
3. in a terminal, ``` cd Debug ``` 
4. ``` sudo ./Beetle ```  

##### Python tcp client
By default, the tcp server runs on port 5000. Passing ```--tcp-port``` sets a different port. ```BeetleClient.py``` in the ```client``` directory reads input provided by the user in hex, and prints responses from the server.

##### Params for tcp connection
Parameters are key value pairs sent at the beginning of the connection in ASCII text. 

* client name *(connect with the client protocol, where name is the name of the client)*

* server bool *(where bool is whether the client is running a GATT server)*

* gateway name *(connect with the gateway protocol, where name is the name of the gateway)*

* device id *(where id is the id of the device at the gateway)*

##### Notes
The main function is in Beetle.cpp.