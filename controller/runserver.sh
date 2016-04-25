#!/bin/bash
./manage.py runserver 0.0.0.0:80 &
./manage.py runsslserver 0.0.0.0:443

pkill -f './manage.py runserver'
pkill -f './manage.py runsslserver'