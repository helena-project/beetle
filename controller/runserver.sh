#!/bin/bash

celery worker -A main -B -l info &
./manage.py runmanager &
sudo ./manage.py runserver 0.0.0.0:80 &

if [ "$#" -eq 2 ];
then
	./manage.py runsslserver 0.0.0.0:443 --certificate "$1" --key "$2" &
	./manage.py runsslserver 0.0.0.0:3003 --certificate "$1" --key "$2"
else
	./manage.py runsslserver 0.0.0.0:443 --certificate certs/cert.pem --key certs/key.pem &
	./manage.py runsslserver 0.0.0.0:3003 --certificate certs/cert.pem --key certs/key.pem
fi

pkill -f 'celery worker -A main'
pkill -f './manage.py runmanager'
pkill -f './manage.py runserver'
pkill -f './manage.py runsslserver'
