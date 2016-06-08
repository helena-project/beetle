#!/bin/bash

# TODO this does not serve static files

NUM_WORKERS=4

celery worker -A main -B -l info &
./manage.py runmanager &

if [ "$#" -eq 2 ];
then
	gunicorn --env DJANGO_SETTINGS_MODULE=main.settings main.wsgi -w $NUM_WORKERS -b 0.0.0.0:80 -b 0.0.0.0:443 -b 0.0.0.0:3003 --certfile="$1" --keyfile="$2"
else
	gunicorn --env DJANGO_SETTINGS_MODULE=main.settings main.wsgi -w $NUM_WORKERS -b 0.0.0.0:80 -b 0.0.0.0:443 -b 0.0.0.0:3003 --certfile="certs/cert.pem" --keyfile="certs/key.pem"
fi

pkill -f 'celery worker -A main'
pkill -f 'gunicorn --env DJANGO_SETTINGS_MODULE=main.settings main.wsgi'
