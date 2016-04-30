#!/bin/bash

./manage.py runserver 0.0.0.0:80 &

if [ "$#" -eq 2 ];
then
	./manage.py runsslserver 0.0.0.0:443 --certificate "$1" --key "$2"
else
	./manage.py runsslserver 0.0.0.0:443
fi

pkill -f './manage.py runserver'
pkill -f './manage.py runsslserver'
