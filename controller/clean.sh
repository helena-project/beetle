#!/bin/bash

read -r -p "This OBLITERATES all state. Are you sure? [y/N] " response
if [[ $response =~ ^([yY][eE][sS]|[yY])$ ]]
then
	rm db.sqlite3
	rm -rf */migrations/
	rm celery-schedule
fi

