# Beetle C++ Gateway for Linux
Beetle is an operating system service that mediates access between applications
and Bluetooth Low Energy peripherals using GATT (Generic Attribute Profile), 
the application-level protocol of Bluetooth Low Energy. While GATT was designed 
for low-power personal area networks, its connection-oriented interface, naming 
hierarchy, and transactional semantics give sufficient structure for an OS to 
manage and understand application behavior and properly manage access to 
peripherals without needing to understand device specific functionality.

## Requirements
- libboost-all-dev, libbluetooth-dev, libcppnetlib-dev, libasio-dev, openssl
- for controller functionality, libcppnetlib may need to be compiled and 
 manually: https://github.com/cpp-netlib/cpp-netlib.git

## To build and run
1. clone and import the project into Eclipse (optional)
2. in a terminal, ``` cd Debug ```
3. ```make all```
4. ```sudo ./Beetle``` 
5. For more options use ```--help```, ```-p``` to print a sample configuration 
file, or write your own configuration

## Commands
Running Beetle presents a shell interface with several commands. Enter 
```help``` to list commands and their explanations.

## TCP connection protocol
Parameters are key value pairs sent at the beginning of the connection in 
ASCII text. 

* ```client name``` *(connect with the client protocol, where name is the 
name of the client)*
* ```server bool``` *(where bool is whether the client is running a GATT 
server)*
* ```gateway name``` *(connect with the gateway protocol, where name is the 
name of the gateway)*
* ```device id``` *(where id is the id of the device at the gateway)*

## Certs directory
This contains default certificates for testing. Needed for openssl to work.

## Code style
Lines are limited to 120 characters.