#!/bin/bash
# set -x


workloads=/home/lupones/manager/experiments/workloads.yaml
inputdir=/home/lupones/manager/experiments/
outputdir=/home/lupones/manager/experiments/outputCSVfiles/numWaysDataGraphs

sudo python3 ./numWays-data-tables.py -w $workloads -id $inputdir -od $outputdir -p hg -p np -dc Total -dc Interval
	






