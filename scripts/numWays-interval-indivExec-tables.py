import argparse
import numpy as np
import os
import pandas as pd
import re
import scipy.stats
import sys
import yaml
import glob

def main():
    parser = argparse.ArgumentParser(description='Process results of workloads by intervals.')
    parser.add_argument('-w', '--workloads', required=True, help='.yaml file where the list of workloads is found.')
    parser.add_argument('-od', '--outputdir', default='./output', help='Directory where output files will be placed')
    parser.add_argument('-id', '--inputdir', default='./data', help='Directory where input are found')
    parser.add_argument('-dc', '--dataCollection', action='append', default=[], help='How data must be collected. Options: Total,Inteval.')

    args = parser.parse_args()
    Z = 1.96  # value of z at a 95% c.i.
    NUM_REPS_APP = 3  # num of values used to calculate the mean value
    SQRT_NUM_REPS_APP = np.sqrt(NUM_REPS_APP)
    NUM_REPS_APP_2 = 6
    SQRT_NUM_REPS_APP_2 = np.sqrt(NUM_REPS_APP_2)


    #print(args.functions)

    for datac in args.dataCollection:
        assert( (datac == "Total") | (datac =="Interval") )

    #print(args.workloads)

    with open(args.workloads, 'r') as f:
        workloads = yaml.load(f)

    outputPath= os.path.abspath(args.outputdir)
    os.makedirs(os.path.abspath(outputPath), exist_ok=True)
    #print(outputPath)
	
    for datac in args.dataCollection:

        outputPathFile = outputPath+ "/npIndiv" + datac
        outputPathFile = os.path.abspath(outputPathFile)
        os.makedirs(os.path.abspath(outputPathFile), exist_ok=True)
        #print(outputPathWorkload)
 
        for wl_id, wl in enumerate(workloads):

            if wl[0].find('.')==-1:
                appName = wl[0] + "_base"
            else: 
                appName = wl[0]

            print(appName)
            wl_in_path = args.inputdir + "/outputFilesMean/" + wl[0] + "/00_" + appName + ".0.csv"
            #print(wl_name) 
                
            dfApp = pd.read_table(wl_in_path, sep=",")  
            dfApp = dfApp[["interval","ipnc:mean","ipnc:std","ev0:mean","ev0:std","l3_kbytes_occ:mean","l3_kbytes_occ:std"]] 
            print("dfApp BEFORE performing any calculation")
            print(dfApp) 
                  
            # calculate confidence interval of IPC
            dfApp["ipnc:std"] =  Z * ( dfApp["ipnc:std"] / SQRT_NUM_REPS_APP )
            dfApp = dfApp.rename(columns={"ipnc:mean": "IPC:mean", "ipnc:std": "IPC:ci"})
 
            # calculate hits/storage
            relErrEV0 = dfApp["ev0:std"] / dfApp["ev0:mean"]
            relErrL3OCCUP = dfApp["l3_kbytes_occ:std"] / dfApp["l3_kbytes_occ:mean"]
            relErrDiv = np.sqrt((relErrEV0*relErrEV0) + (relErrL3OCCUP*relErrL3OCCUP))

            dfApp["ev0:mean"] = dfApp["ev0:mean"] / dfApp["l3_kbytes_occ:mean"]
            dfApp["ev0:std"] = relErrDiv * dfApp["ev0:mean"]
            dfApp["ev0:std"] =  Z * ( dfApp["ev0:std"] / SQRT_NUM_REPS_APP_2 )

            dfApp = dfApp.rename(columns={'ev0:mean': 'hits/storage:mean', 'ev0:std': 'hits/storage:ci'})

            # calculate number of ways 
            dfApp["l3_kbytes_occ:mean"] = dfApp["l3_kbytes_occ:mean"] / 1024
            dfApp["l3_kbytes_occ:std"] =  Z * ( dfApp["l3_kbytes_occ:std"] / SQRT_NUM_REPS_APP )

            dfApp = dfApp.rename(columns={"l3_kbytes_occ:mean": "numberOfWays:mean", "l3_kbytes_occ:std": "numberOfWays:ci"})
               
            # save table with number of ways in x-axis
            dfAppNW = dfApp[["numberOfWays:mean","IPC:mean","IPC:ci","hits/storage:mean","hits/storage:ci"]]
            dfAppNW = dfAppNW.set_index(['numberOfWays:mean'])
            print("dfApp with number of ways")
            print(dfAppNW)
            outputPathAppFile = outputPathFile + "/" + wl[0] + "-numWaysDataTable.csv"
            print(outputPathAppFile)
            dfAppNW.to_csv(outputPathAppFile, sep=',')

            # save table with interval in x-axis
            dfAppIN = dfApp[["interval","IPC:mean","IPC:ci","hits/storage:mean","hits/storage:ci"]]
            dfAppIN = dfAppIN.set_index(['interval'])
            print("dfApp with interval")
            print(dfAppIN)
            outputPathAppFile = outputPathFile + "/" + wl[0] + "-intervalDataTable.csv"
            print(outputPathAppFile)
            dfAppIN.to_csv(outputPathAppFile, sep=',')



# El main es crida des d'ací
if __name__ == "__main__":
    main()

