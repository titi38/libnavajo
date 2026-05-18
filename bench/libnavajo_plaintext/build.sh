#!/bin/sh
g++ -O3 -DNDEBUG -std=c++17 bench_libnavajo.cc -o bench_libnavajo   -lnavajo -lpthread -lssl -lcrypto -lz

