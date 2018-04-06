#!/bin/bash

DIRS=()
for arg in $@; do
	if [ "${arg}" == "--" ]; then
		shift
		break
	else
		DIRS+=("${arg}")
		shift
	fi
done

if [ ${#DIRS[@]} -eq 0 ]; then
	echo "Usage: script <dir> [DIR]... <-- workloads [optional args]>"
	exit 1
fi

if [ $# -eq 0 ]; then
	echo "No workloads provided"
	exit 1
fi

WORKLOADS=$(readlink -f $1)
shift
LAUNCH_ARGS=$@

for ((i=0; i < ${#DIRS[@]}; i++)); do
	echo ${DIRS[$i]}
	cd ${DIRS[$i]}
	bash ~/manager/scripts/launch.bash $WORKLOADS ${LAUNCH_ARGS}
	cd -
done
