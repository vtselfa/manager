#!/bin/bash

function join_by { local IFS="$1"; shift; echo "$*"; }

getopt --test > /dev/null
if [[ $? -ne 4 ]]; then
    echo "I’m sorry, `getopt --test` failed in this environment."
    exit 1
fi

OPTIONS=
LONGOPTIONS=max-rep:,ini-rep:,clog-min:

# -temporarily store output to be able to check for errors
# -e.g. use “--options” parameter by name to activate quoting/enhanced mode
# -pass arguments only via   -- "$@"   to separate them correctly
PARSED=$(getopt --options=$OPTIONS --longoptions=$LONGOPTIONS --name "$0" -- "$@")
if [[ $? -ne 0 ]]; then
    # e.g. $? == 1
    #  then getopt has complained about wrong arguments to stdout
    exit 2
fi
# read getopt’s output this way to handle the quoting right:
eval set -- "$PARSED"

# now enjoy the options in order and nicely split until we see --
INI_REP=0
MAX_REP=3
CLOG_MIN=war
while true; do
    case "$1" in
        --max-rep)
            MAX_REP="$2"
            shift 2
            ;;
        --ini-rep)
            INI_REP="$2"
            shift 2
            ;;
        --clog-min)
            CLOG_MIN="$2"
            shift 2
            ;;
        --)
            shift
            break
            ;;
        *)
            echo "Programming error"
			echo $@
            exit 3
            ;;
    esac
done

# handle non-option arguments
if [[ $# -ne 1 ]]; then
    echo "$0: A single input file is required."
	echo $@
    exit 4
fi

WORKLOADS=$1



DIR=$(dirname $0)

source ${DIR}/config.bash

# Enable prefetchers
wrmsr -a 0x1A4 0x0

# Set performance governor
echo performance | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor > /dev/null
echo 2200000 | tee /sys/devices/system/cpu/cpufreq/policy*/scaling_min_freq > /dev/null

# Save manager binary
cp ${HOME}/manager/manager .

mkdir -p data
mkdir -p configs
mkdir -p log

for ((REP=${INI_REP};REP<${MAX_REP};REP++)); do
	for MASK in ${MASKS[@]}; do
		while read WL; do
			WL=$(echo $WL | tr '\-[],' " ")

			if [ ${#MASKS[@]} -eq 1 ]; then
				ID=$(join_by - ${WL[@]})
			else
				ID=$(join_by - ${WL[@]})_${MASK}
			fi

			CONFIG=configs/${ID}.yaml
			OUT=data/${ID}_${REP}.csv
			FIN_OUT=data/${ID}_${REP}_fin.csv
			TOT_OUT=data/${ID}_${REP}_tot.csv
			LOG=log/${ID}_${REP}.log

			echo $WL $((REP+1))/${MAX_REP} 0x${MASK}
			python3 ${DIR}/makoc.py template.mako --lookup "${DIR}/templates" --defs '{apps: ['$(join_by , ${WL[@]})'], mask: '$MASK'}'> ${CONFIG} || exit 1

			./manager --config ${CONFIG} -o ${OUT} --fin-out ${FIN_OUT} --total-out ${TOT_OUT} --flog-min dbg --clog-min ${CLOG_MIN} --log-file $LOG --cat-impl linux

		done < $WORKLOADS
	done
done

echo 1200000 | tee /sys/devices/system/cpu/cpufreq/policy*/scaling_min_freq > /dev/null

