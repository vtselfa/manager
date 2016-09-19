#!/bin/bash
# set -x
#Events:
#OFFCORE_RESPONSE.DEMAND_DATA_RD.LLC_MISS.ANY_RESPONSE
#L2_RQSTS.DEMAND_DATA_RD_MISS
#L1D.REPLACEMENT

#import dictionaries 
source args.sh
source path.sh

#bucle de asignacion de numero de vias de LLC
for vias in 0x00003 0x00007 0x0000F 0x0001F 0x0003F 0x0007F 0x000FF 0x001FF 0x003FF 0x007FF 0x00FFF 0x01FFF 0x03FFF 0x07FFF 0x0FFFF 0x1FFFF 0x3FFFF 0x7FFFF 0xFFFFF
do

sudo pqos -e "llc:1=$vias;"
sudo pqos -a "llc:1=23;"

#key=$i
#value=${array[$i]}
for i in "${!PATHS[@]}"
do

#creation of $i.log = contains counters every second (app in core 23)
sudo ./pcm-core.x 1 -csv="$i-$vias.log" -e cpu/umask=0x01,event=0xB7,name=OFFCORE_RESPONSE.DEMAND_DATA_RD.LLC_MISS.ANY_RESPONSE,offcore_rsp=0x3fffc00001/ -e cpu/umask=0x21,event=0x24,name=L2_RQSTS.DEMAND_DATA_RD_MISS/ -e cpu/umask=0x01,event=0x51,name=L1D.REPLACEMENT/ -- taskset -c 23 ${PATHS[$i]}/$i ${ARGS[$i]}

#period of CPU
let a=10**9
a=$(echo print $a*2.2 | python)
periodu=$(echo print 1000/$a | python)


#creation of $i.csv file 
#MPKI = Misses per kilo-instruction
#MPuS = Misses per microsecond

#awk -F , '/^Time elapsed/ {print $0;}' $i.log | grep -o '[0-999]*' >"test$i.csv"
#paste -d , test$i.csv a$i.csv >$i.csv


(
echo "IPC,MissesL3,MissesL2,MissesL1,MPKIL3,MPKIL2,MPKIL1,MPuL3,MPuL2,MPuL1,Cycles"
awk -F , '/^23/ {printf("%s,%d,%d,%d,%f,%f,%f,%f,%f,%f,%d\n", $2,$5,$6,$7,$5/$3*1000,$6/$3*1000,$7/$3*1000,$5/$periodu,$6/$periodu,$7/$periodu,$4)}' $i-$vias.log
) >"a$i.csv"

#seconds column
S=$(cat a$i.csv | awk 'NR>1' | wc -l)
(echo "Seconds"; seq 1 $S) >"test$i.csv"

#merge in a single doc seconds + rest of columns
paste -d , test$i.csv a$i.csv >b$i.csv

#remove auxiliary files used
rm test$i.csv
rm a$i.csv

#calculate sum of cycles column
(awk -F , '{s+=$i2} END {printf ",,,,,,,,,,%d\n", s}' b$i.csv) >test$i.csv

#add total num of cycles in a final row to the previous table
(
cat b$i.csv 
cat test$i.csv
) >$i-$vias.csv

#remove auxiliary file used 
rm b$i.csv

#move files to plots directory 
#mv $i-$vias.csv /home/lupones/plots/
#mv $i-$vias.log /home/lupones/plots/

done

sudo pqos -R #reset CAT

done
