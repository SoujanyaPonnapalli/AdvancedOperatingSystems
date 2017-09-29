#!/bin/bash

gcc perf-tools.c

./a.out 0 0 0 0 > 0.txt
./a.out 0 0 0 1 > 1.txt
./a.out 0 0 1 0 > 2.txt
./a.out 0 0 1 1 > 3.txt
./a.out 0 1 0 0 > 4.txt
./a.out 0 1 0 1 > 5.txt
./a.out 0 1 1 0 > 6.txt
./a.out 0 1 1 1 > 7.txt
./a.out 1 0 0 0 > 8.txt
./a.out 1 0 0 1 > 9.txt
./a.out 1 0 1 0 > 10.txt
./a.out 1 0 1 1 > 11.txt
./a.out 1 1 0 0 > 12.txt
./a.out 1 1 0 1 > 13.txt
./a.out 1 1 1 0 > 14.txt
./a.out 1 1 1 1 > 15.txt
