#!/bin/bash

COMMON_NAME=$1

ROOT_CERT=rootCA

COUNTRY=US
STATE=California
LOCALITY=Santa\ Clara
ORGANIZATION=beetle
ORGANIZATIONAL_UNIT=beetle
EMAIL=none@none.com

openssl genrsa -out $COMMON_NAME.key 2048
openssl req -new -key $COMMON_NAME.key -out $COMMON_NAME.csr -subj "/C=$COUNTRY/ST=$STATE/L=$LOCALITY/O=$ORGANIZATION/OU=$ORGANIZATIONAL_UNIT/CN=$COMMON_NAME/emailAddress=$EMAIL"
openssl x509 -req -in $COMMON_NAME.csr -CA $ROOT_CERT.pem -CAkey $ROOT_CERT.key -CAcreateserial -out $COMMON_NAME.crt -days 365 -sha256 