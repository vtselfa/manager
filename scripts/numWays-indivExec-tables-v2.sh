#!/bin/bash
# set -x


workloads=/home/lupones/manager/experiments/indiv-workloads.yaml
inputdir=/home/lupones/manager/experiments/indivExec
outputdir=/home/lupones/manager/experiments/outputCSVfiles/indivExecGraphs

sudo python3 ./numWays-indivExec-tables-v2.py -w $workloads -id $inputdir -od $outputdir -dc Total 
	






