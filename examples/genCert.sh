#!/bin/sh

openssl req -x509 -nodes -days 365 -newkey rsa:2048 -keyout mykey.pem -out mycert.pem
cat mykey.pem >> mycert.pem
rm -f mykey.pem
