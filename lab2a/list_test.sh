#!/bin/bash

#1
for threads in 1; do
    for iter in 10 100 1000 10000 20000; do
        ./lab2_list --threads=$threads --iter=$iter >>lab2_list.csv
    done
done

#2
for threads in 2 4 8 12; do
    for iter in 1 10 100 1000; do
	./lab2_list --threads=$threads --iter=$iter >>lab2_list.csv
    done
done
for threads in 2 4 8 12; do
    for iter in 1 2 4 8 16 32; do
	./lab2_list --threads=$threads --iter=$iter --yield=i >>lab2_list.csv
	./lab2_list --threads=$threads --iter=$iter --yield=d >>lab2_list.csv
	./lab2_list --threads=$threads --iter=$iter --yield=il >>lab2_list.csv
	./lab2_list --threads=$threads --iter=$iter --yield=dl >>lab2_list.csv
    done
done

#3
for threads in 12; do
    for iter in 32; do
	./lab2_list --threads=$threads --iter=$iter --yield=i --sync=m >>lab2_list.csv
	./lab2_list --threads=$threads --iter=$iter --yield=i --sync=s >>lab2_list.csv
	./lab2_list --threads=$threads --iter=$iter --yield=d --sync=m >>lab2_list.csv
	./lab2_list --threads=$threads --iter=$iter --yield=d --sync=s >>lab2_list.csv
	./lab2_list --threads=$threads --iter=$iter --yield=il --sync=m >>lab2_list.csv
	./lab2_list --threads=$threads --iter=$iter --yield=il --sync=s >>lab2_list.csv
	./lab2_list --threads=$threads --iter=$iter --yield=dl --sync=m >>lab2_list.csv
	./lab2_list --threads=$threads --iter=$iter --yield=dl --sync=s >>lab2_list.csv
    done
done

#4
for threads in 1 2 4 8 12 16 24; do
    for iter in 1000; do
	./lab2_list --threads=$threads --iter=$iter --sync=s >>lab2_list.csv
	./lab2_list --threads=$threads --iter=$iter --sync=m >>lab2_list.csv
    done
done
