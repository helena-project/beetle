# Beetle TCP client

By default, the tcp server runs on port 5000. Passing ```--tcp-port``` sets a different port. ```BeetleClient.py``` reads input provided by the user, and prints responses from the server.

##### Usage

The tcp client will first ask for connection params. See Beetle readme for params. These are read line by line as 'key value' pairs.

To send requests:

* ```read handleNo```
* ```write handleNo FFFF...``` *(spaces are ignored in the hex portion)*
* ```FFFF...``` *(directly send hex)*