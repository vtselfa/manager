#!/bin/bash

DIR=$(dirname $0)

source ${DIR}/config.bash

# Enable prefetchers
wrmsr -a 0x1A4 0x0

# Set performance governor
echo performance | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor > /dev/null
echo 2200000 | tee /sys/devices/system/cpu/cpufreq/policy*/scaling_min_freq > /dev/null

# Save manager binary
cp /home/viselol/manager/manager .

EVENTS=${EVENTS-cpu/umask=0x04,event=0xD1,name=MEM_LOAD_UOPS_RETIRED.L3_HIT/ -e cpu/umask=0x20,event=0xD1,name=MEM_LOAD_UOPS_RETIRED.L3_MISS/ -e cpu/umask=0x04,event=0xA3,name=CYCLE_ACTIVITY.STALLS_TOTAL,cmask=4/ -e cpu/umask=0x06,event=0xA3,name=CYCLE_ACTIVITY.STALLS_LDM_PENDING,cmask=6/}

echo Events: ${EVENTS}

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

			./manager --config ${CONFIG} --ti ${INT} --mi ${MAX} -e ${EVENTS} -o ${OUT} --fin-out ${FIN_OUT} --total-out ${TOT_OUT} --flog-min dbg --log-file $LOG

		done < $1
	done
done

echo 1200000 | tee /sys/devices/system/cpu/cpufreq/policy*/scaling_min_freq > /dev/null

