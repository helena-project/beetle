#!/bin/bash

./manage.py runserver 0.0.0.0:80 &

if [ "$#" -eq 2 ];
then
	./manage.py runsslserver 0.0.0.0:443 --certificate "$1" --key "$2" &
	./manage.py runsslserver 0.0.0.0:3003 --certificate "$1" --key "$2"
else
	./manage.py runsslserver 0.0.0.0:443 &
	./manage.py runsslserver 0.0.0.0:3003
fi

pkill -f './manage.py runserver'
pkill -f './manage.py runsslserver'
