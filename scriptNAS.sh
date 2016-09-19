#Events:
#OFFCORE_RESPONSE.DEMAND_DATA_RD.LLC_MISS.ANY_RESPONSE
#L2_RQSTS.DEMAND_DATA_RD_MISS
#L1D.REPLACEMENT

#creation of $1.log = contains counters every second (app in core 23) 
sudo ./pcm-core.x 1 -csv="$1.log" -e cpu/umask=0x01,event=0xB7,name=OFFCORE_RESPONSE.DEMAND_DATA_RD.LLC_MISS.ANY_RESPONSE,offcore_rsp=0x3fffc00001/ -e cpu/umask=0x21,event=0x24,name=L2_RQSTS.DEMAND_DATA_RD_MISS/ -e cpu/umask=0x01,event=0x51,name=L1D.REPLACEMENT/ -- taskset -c 23 /home/benchmarks/NPB3.3.1/NPB3.3-SER/bin/$1 

#period of CPU
let a=10**9
a=$(echo print $a*2.2 | python)
periodu=$(echo print 1000/$a | python)


#creation of $1.csv file 
#MPKI = Misses per kilo-instruction
#MPuS = Misses per microsecond

#awk -F , '/^Time elapsed/ {print $0;}' $1.log | grep -o '[0-999]*' >"test$1.csv"
#paste -d , test$1.csv a$1.csv >$1.csv


(
echo "IPC,MissesL3,MissesL2,MissesL1,MPKIL3,MPKIL2,MPKIL1,MPuL3,MPuL2,MPuL1,Cycles"
awk -F , '/^23/ {printf("%s,%d,%d,%d,%f,%f,%f,%f,%f,%f,%d\n", $2,$5,$6,$7,$5/$3*1000,$6/$3*1000,$7/$3*1000,$5/$periodu,$6/$periodu,$7/$periodu,$4)}' $1.log
) >"a$1.csv"

#seconds column
S=$(cat a$1.csv | awk 'NR>1' | wc -l)
(echo "Seconds"; seq 1 $S) >"test$1.csv"

#merge in a single doc seconds + rest of columns
paste -d , test$1.csv a$1.csv >b$1.csv

#remove auxiliary files used
rm test$1.csv
rm a$1.csv

#calculate sum of cycles column
(awk -F , '{s+=$12} END {printf ",,,,,,,,,,%d\n", s}' b$1.csv) >test$1.csv

#add total num of cycles in a final row to the previous table
(
cat b$1.csv 
cat test$1.csv
) >$1.csv

#remove auxiliary file used 
rm b$1.csv

#move files to plots directory 
mv $1.csv /home/lupones/plots/$1.csv
mv $1.log /home/lupones/plots/$1.log

