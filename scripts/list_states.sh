#!/bin/bash

# $1 = experiment (E.g 170919)



inputdir=/home/lupones/manager/experiments/$1/criticalAlone/log
outputdir=/home/lupones/manager/experiments/outputCSVfiles/$1/criticalAlone/AAA-states/

mkdir $outputdir

for filename in $inputdir/*.log; do
    
    echo $filename
  
    touch aux.dat
    cat $filename | grep -E 'Current state' > aux.dat

    fn=$(echo $filename | awk -F'/' '{print $9}')
	fn=${fn::-4}
    fn=$(echo $outputdir$fn".states")
    

    echo $fn
    touch $fn

    while read l; do
        echo $l | tail -c 3 >> $fn
    done <aux.dat

    rm aux.dat


done 


