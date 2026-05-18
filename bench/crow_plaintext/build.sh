#!/bin/sh
g++ -O3 -std=c++17 bench_crow.cc -o bench_crow \
  -I ~/Crow/include -lpthread
