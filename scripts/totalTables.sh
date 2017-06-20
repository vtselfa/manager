#!/bin/bash
# set -x


workloads=/home/lupones/manager/experiments/workloads.yaml
inputdir=/home/lupones/manager/experiments/
outputdir=/home/lupones/manager/experiments/outputCSVfiles/totalGraphs

sudo python3 ./totalTables.py -w $workloads -f ipc -f hits -id $inputdir -od $outputdir -p hg -p np -dc Total -dc Interval
	






