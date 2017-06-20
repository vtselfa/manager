# !/bin/bash

workloads=/home/lupones/manager/experiments/workloads.yaml
inputdir=/home/lupones/manager/experiments/
outputdir=/home/lupones/manager/experiments/


function join_by { local IFS="$1"; shift; echo "$*"; }

cd $inputdir
for experiment in hgTotal hgInterval npTotal npInterval; do
        result=$(pwd)"/"$experiment"/outputCSVInterval/"
        cd $result
	
	# loop over all the directories 
	for d in */ ; do
		#loop each app
		cd $d
		apps=$(echo $d | tr -d '/')
		# IPC graph command
		IPC=$(echo python3 ./simplot.py -g 2 2 -g 2 2 -o ./IPC-$apps.pdf)
		# Hits L3 graph command
		HITSL3=$(echo python3 ./simplot.py -g 2 2 -g 2 2 -o ./HITSL3-$apps.pdf)
		# Hits per occup. L3 graph command
		HITSPEROCCUPL3=$(echo python3 ./simplot.py -g 2 2 -g 2 2 -o ./HITSOCCUPL3-$apps.pdf)

		for i in $(echo $d | tr "-" "\n")
		do
  			app=$(echo $i | tr -d '/')
			for j in *$app*; do
				if [[ $j == *"hitsL3"* ]];then
					#create graph hits L3
					echo $j
				fi
				if [[ $j == *"hitsperOccupL3"* ]];then
                                        #create graph hits per occup. L3
					echo $j
                                fi
				if [[ $j == *"IPC"* ]];then
                                        #create graph IPC
					IPC=$IPC" "$(echo --plot '{kind: line, datafile: '"$j"', index: 0, cols: [1], ylabel: IPC, xlabel: interval} ')
					echo $IPC
					exit
                                fi
			done
		done
		exit
	done
done 
exit


# EJEMPLO
# cd /home/lupones/manager/experiments/hgTotal/outputCSVInterval/astar-gobmk-h264ref-leslie3d-namd-tonto-zeusmp-bt.C
# python3 /home/lupones/manager/scripts/simplot.py -g 1 1 -o ./IPC-astar-gobmk-h264ref-leslie3d-namd-tonto-zeusmp-bt.C.pdf --plot {kind: line, datafile: 00_astar_base_IPC.csv, index: 0, cols: [1], ylabel: IPC, xlabel: interval}






