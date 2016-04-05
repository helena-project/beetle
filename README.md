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

##### Notes
The main function is in Beetle.cpp.