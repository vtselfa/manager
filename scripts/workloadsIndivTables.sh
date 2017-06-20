#!/bin/bash
set -x


workloads=/home/lupones/manager/experiments/workloads.yaml
inputdir=/home/lupones/manager/experiments/
outputdir=/home/lupones/manager/experiments/outputCSVfiles/workloadsGraphs/


sudo python3 ./workloadsIndivTables.py -w $workloads -f ipc -f hits -f occup -id $inputdir -od $outputdir -p hg -p np -dc Total -dc Interval
	






