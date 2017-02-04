# Beetle

The next generation of computing peripherals will be low-power ubiquitous
computing devices such as door locks, smart watches, and heart rate monitors.
Bluetooth Low Energy is a primary protocol for connecting such peripherals to
mobile and gateway devices. Current operating system support for Bluetooth Low
Energy forces peripherals into vertical application silos. As a result, simple,
intuitive applications such as opening a door with a smart watch or
simultaneously logging and viewing heart rate data are impossible. Beetle is a
new hardware interface that virtualizes peripherals at the application layer,
allowing safe access by multiple programs without requiring the operating
system to understand hardware functionality, fine-grained access control to
peripheral device resources, and transparent access to peripherals connected
over the network.

You can read an overview of Beetle's design in our [MobiSys 2016
paper](http://iot.stanford.edu/pubs/beetle-mobisys16.pdf).

## Beetle C++ gateway
In ```gateway``` subdirectory.

## Python tcp client
In ```pyclient``` subdirectory.

## Python gatt library
In ```pygatt``` subdirectory.

## Python controller
In ```controller``` subdirectory. Needed to run with the controller enabled.

## Gateway applications
In ```pyapps``` supdirectory.

## Certs directory
Basic scripts to generate self-signed certificates for Beetle.

# License
Copyright 2016 James Hong, Amit Levy

Licensed under the Apache License, Version 2.0 (the "License"); you may not use
this file except in compliance with the License. You may obtain a copy of the
License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied. See the License for the
specific language governing permissions and limitations under the License.

This product bundles nlohmann/json and bootstrap, which are available under
MIT Licenses. For details, see gateway/lib/include/json/ and
https://getbootstrap.com/.
