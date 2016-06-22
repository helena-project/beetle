#!/bin/bash

sudo apt-get install libboost-all-dev libbluetooth-dev libasio-dev openssl

sudo apt-get install cmake doxygen

rm -r deps
mkdir deps

cd deps

git clone https://github.com/glynos/cpp-netlib.git
cd cpp-netlib

git checkout 0.13-release
git submodule init
git submodule update

mkdir build
cd build
cmake ..

make all $1
make test

read -r -p "Confim installation of cpp-netlib? [y/N] " response
if [[ $response =~ ^([yY][eE][sS]|[yY])$ ]]
then
    sudo make install
fi
