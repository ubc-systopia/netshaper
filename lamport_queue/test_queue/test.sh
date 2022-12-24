#!/bin/bash

# Define a variable as the number of execution times

make clean
# make producer and consumer
make

# Initialize the queue
./initializer  
./producer &
./consumer &

wait

make clean