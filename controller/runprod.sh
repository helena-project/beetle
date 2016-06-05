#!/bin/bash

# TODO not working yet
NUM_WORKERS=8

gunicorn --env DJANGO_SETTINGS_MODULE=main.settings main.wsgi -w $NUM_WORKERS \
-b 0.0.0.0:80 -b 0.0.0.0:443 --do-handshake-on-connect

