#!/bin/bash
# set -x

# $1 = experiment 
# E.g 170719

workloads=/home/lupones/manager/experiments/$1/workloads$1.yaml
inputdir=/home/lupones/manager/experiments/$1/
outputdir=/home/lupones/manager/experiments/outputCSVfiles/$1/

for i in 0,0 20,20; do
    listways=$listways" -lw ("$i") "
done

sudo python3 ./1710-interval-total-tables.py -w $workloads -id $inputdir -od $outputdir $listways
