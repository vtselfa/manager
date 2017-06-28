#!/bin/bash
# set -x


workloads=/home/lupones/manager/experiments/indiv-workloads.yaml
inputdir=/home/lupones/manager/experiments/npIndivTotal
outputdir=/home/lupones/manager/experiments/outputCSVfiles/numWaysDataGraphs

sudo python3 ./numWays-interval-indivExec-tables.py -w $workloads -id $inputdir -od $outputdir -dc Total 
	






