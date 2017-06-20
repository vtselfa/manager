#!/bin/bash
# set -x


workloads=/home/lupones/manager/experiments/workloads.yaml
inputdir=/home/lupones/manager/experiments/
outputdir=/home/lupones/manager/experiments/outputCSVfiles/slowdownGraphs

sudo python3 ./slowdownTables.py -w $workloads -id $inputdir -od $outputdir -p hg -p np -dc Total -dc Interval
	






