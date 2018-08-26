#!/bin/bash

TEMP_FILE=temp.csv
END_FILE=lab2b_list.csv


#Clear files
rm -f $TEMP_FILE $END_FILE


#1
for threads in 1 2 4 8 12 16 24; do
    for iter in 1000; do
        ./lab2_list --threads=$threads --iter=$iter --sync=m >>$TEMP_FILE
        ./lab2_list --threads=$threads --iter=$iter --sync=s >>$TEMP_FILE
    done
done

#2
for threads in 1 2 4 8 16 24; do
    for iter in 1000; do
	./lab2_list --threads=$threads --iter=$iter --sync=m >>$TEMP_FILE
    done
done

#3
for threads in 1 4 8 12 16; do
    for iter in 1 2 4 8 16; do
	./lab2_list --threads=$threads --iter=$iter --lists=4 --yield=id >>$TEMP_FILE
    done
done
for threads in 1 4 8 12 16; do
    for iter in 10 20 40 80; do
	./lab2_list --threads=$threads --iter=$iter --lists=4 --yield=id --sync=m >>$TEMP_FILE
	./lab2_list --threads=$threads --iter=$iter --lists=4 --yield=id --sync=s >>$TEMP_FILE
    done
done

#4/5
for threads in 1 2 4 8 12; do
    for lists in 1 4 8 16; do
	./lab2_list --threads=$threads --iter=1000 --lists=$lists --sync=m >>$TEMP_FILE
	./lab2_list --threads=$threads --iter=1000 --lists=$lists --sync=s >>$TEMP_FILE
    done
done

#Eliminate duplicate tests
cat $TEMP_FILE | while read line
do
    if [ -z $(echo $line | sed 's/,[0-9]*,[0-9]*,[0-9]*,[0-9]*$//' | grep -sf - $END_FILE) ]; then
	echo $line >>$END_FILE
    fi
done

rm $TEMP_FILE
