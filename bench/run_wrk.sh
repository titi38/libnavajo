#!/bin/sh
wrk -t8 -c32 -d30s --latency http://127.0.0.1:8080/plaintext
wrk -t8 -c64 -d30s --latency http://127.0.0.1:8080/plaintext
wrk -t8 -c128 -d30s --latency http://127.0.0.1:8080/plaintext
