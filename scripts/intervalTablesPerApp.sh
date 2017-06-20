#!/bin/bash
# set -x


workloads=/home/lupones/manager/experiments/workloads.yaml
inputdir=/home/lupones/manager/experiments/
outputdir=/home/lupones/manager/experiments/outputCSVfiles/intervalgraphs


sudo python3 ./intervalTablesPerApp.py -w $workloads -f ipc -f hits -f hitsOccup -id $inputdir/$experiment -od $outputdir/$dataC -p hg -p np -dc Total -dc Interval
	






