
#!/bin/bash
set -x

# $1 = name of the experiment
# E.g. 170719

WORKLOADS=/home/lupones/manager/experiments/$1/workloads$1.yaml
DIR=/home/lupones/manager/experiments/$1/


for i in 0,0; do IFS=","; set -- $i;


    INPUT=$DIR$1"cr"$2"others/data/"
    OUTPUT=$DIR$1"cr"$2"others/outputFilesMean/"

	mkdir $OUTPUT

	# files with mean values of data per interval
	python3 process_int.py -w $WORKLOADS -i $INPUT -o $OUTPUT

	# files with total and final values 
	python3 process_tot_fin_int.py -w $WORKLOADS -i $INPUT -o $OUTPUT -ft tot -ft fin

done
