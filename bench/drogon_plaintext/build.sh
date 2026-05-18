#!/bin/sh
g++ -O3 -DNDEBUG -std=c++17 bench_drogon.cc -o bench_drogon \
  -I/usr/include/jsoncpp \
  -ldrogon -ltrantor -lpthread -lssl -lcrypto -lz -luuid -lyaml-cpp -ljsoncpp
