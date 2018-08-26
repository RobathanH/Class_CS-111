#! /usr/bin/gnuplot

# gen plot params
set terminal png
set datafile separator ","


#1
set title "lab2b\_1: Throughput vs Threads"
set xlabel "Threads"
set logscale x 10
set xrange [0.75:]
set ylabel "Throughput (ops / s)"
set logscale y 10
set output 'lab2b_1.png'

#grep into plot
plot \
     "< grep 'list-none-s,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
     title 'Spinlock' with linespoints lc rgb 'red', \
     "< grep 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
     title 'Mutex' with linespoints lc rgb 'green'

#2
set title "lab2b\_2: Wait-For-Lock Time and Average Time Per Operation vs Threads"
set xlabel 'Threads"
set logscale x 10
set xrange [0.75:]
set ylabel "Time (ns)"
set logscale y 10
set output 'lab2b_2.png'

#grep into plot
plot \
     "< grep 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):($8) \
     title 'Wait-For-Lock Time' with linespoints lc rgb 'red', \
     "< grep 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):($7) \
     title 'Time per Operation' with linespoints lc rgb 'green'

#3
set title "lab2b\_3: Successful Runs With Multiple Lists"
set xlabel 'Threads'
set logscale x 10
set xrange [0.75:]
set ylabel 'Iterations'
set logscale y 10
set output 'lab2b_3.png'

#grep into plot
plot \
     "< grep 'list-id-m,[0-9]*,[0-9]*,4,' lab2b_list.csv" using ($2):($3) \
     title 'Mutex' with points lc rgb 'green', \
     "< grep 'list-id-s,[0-9]*,[0-9]*,4,' lab2b_list.csv" using ($2):($3) \
     title 'Spinlock' with points lc rgb 'red', \
     "< grep 'list-id-none,[0-9]*,[0-9]*,4,' lab2b_list.csv" using ($2):($3) \
     title 'No Synchronization' with points lc rgb 'blue'

#4
set title "lab2b\_4: Throughput vs Threads for Mutexes with Different List Granularity"
set xlabel 'Threads'
set logscale x 10
set xrange [0.75:]
set ylabel 'Throughput'
set logscale y 10
set output 'lab2b_4.png'

#grep into plot
plot \
     "< grep 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
     title 'Lists: 1' with linespoints lc rgb 'red', \
     "< grep 'list-none-m,[0-9]*,1000,4,' lab2b_list.csv" using ($2):(1000000000/($7)) \
     title 'Lists: 4' with linespoints lc rgb 'green', \
     "< grep 'list-none-m,[0-9]*,1000,8,' lab2b_list.csv" using ($2):(1000000000/($7)) \
     title 'Lists: 8' with linespoints lc rgb 'blue', \
     "< grep 'list-none-m,[0-9]*,1000,16,' lab2b_list.csv" using ($2):(1000000000/($7)) \
     title 'Lists: 16' with linespoints lc rgb 'yellow'

#5
set title "lab2b\_5: Throughput vs Threads for Spinlocks with Different List Granularity"
set xlabel 'Threads'
set logscale x 10
set xrange [0.75:]
set ylabel 'Throughput'
set logscale y 10
set output 'lab2b_5.png'

#grep into plot
plot \
     "< grep 'list-none-s,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
     title 'Lists: 1' with linespoints lc rgb 'red', \
     "< grep 'list-none-s,[0-9]*,1000,4,' lab2b_list.csv" using ($2):(1000000000/($7)) \
     title 'Lists: 4' with linespoints lc rgb 'green', \
     "< grep 'list-none-s,[0-9]*,1000,8,' lab2b_list.csv" using ($2):(1000000000/($7)) \
     title 'Lists: 8' with linespoints lc rgb 'blue', \
     "< grep 'list-none-s,[0-9]*,1000,16,' lab2b_list.csv" using ($2):(1000000000/($7)) \
     title 'Lists: 16' with linespoints lc rgb 'yellow'