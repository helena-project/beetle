#!/bin/bash

OUTFILE=rootCA

COMMON_NAME=Beetle\ CA
COUNTRY=US
STATE=California
LOCALITY=Santa\ Clara
ORGANIZATION=beetle
ORGANIZATIONAL_UNIT=beetle
EMAIL=none@none.com

openssl genrsa -out $OUTFILE.key 2048
openssl req -x509 -new -nodes -key $OUTFILE.key -sha256 -days 365 -out $OUTFILE.pem -subj "/C=$COUNTRY/ST=$STATE/L=$LOCALITY/O=$ORGANIZATION/OU=$ORGANIZATIONAL_UNIT/CN=$COMMON_NAME/emailAddress=$EMAIL"