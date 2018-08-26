#!/bin/bash

#1
for threads in 2 4 8 12; do
    for iter in 10 20 40 80 100 1000 10000 100000; do
       	./lab2_add --threads=$threads --iter=$iter >>lab2_add.csv
	./lab2_add --threads=$threads --iter=$iter >>lab2_add.csv
    done
done

#2
for threads in 2 8; do
    for iter in 100 1000 10000 100000; do
       	./lab2_add --threads=$threads --iter=$iter >>lab2_add.csv
	./lab2_add --threads=$threads --iter=$iter --yield >>lab2_add.csv
    done
done

#3
for threads in 1; do
    for iter in 10 20 40 80 100 1000 10000 100000; do
       	./lab2_add --threads=$threads --iter=$iter >>lab2_add.csv
    done
done

#4
for threads in 2 4 8 12; do
    ./lab2_add --threads=$threads --iter=10000 --yield >>lab2_add.csv

    ./lab2_add --threads=$threads --iter=10000 --sync=m --yield >>lab2_add.csv

    ./lab2_add --threads=$threads --iter=1000 --sync=s --yield >>lab2_add.csv

    ./lab2_add --threads=$threads --iter=10000 --sync=c --yield >>lab2_add.csv
done

#5
for threads in 1 2 4 8 12; do
    ./lab2_add --threads=$threads --iter=10000 >>lab2_add.csv

    ./lab2_add --threads=$threads --iter=10000 --sync=m >>lab2_add.csv

    ./lab2_add --threads=$threads --iter=1000 --sync=s >>lab2_add.csv

    ./lab2_add --threads=$threads --iter=10000 --sync=c >>lab2_add.csv
done
