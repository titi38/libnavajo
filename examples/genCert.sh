#!/bin/sh

openssl req -x509 -nodes -days 365 -newkey rsa:2048 -keyout mykey.pem -out myCert.pem
cat mykey.pem >> myCert.pem
rm -f mykey.pem
