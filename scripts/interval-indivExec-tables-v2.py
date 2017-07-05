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

    outputPath = os.path.abspath(args.outputdir)
    os.makedirs(os.path.abspath(outputPath), exist_ok=True)
    #print(outputPath)
	
    for datac in args.dataCollection:

        for wl_id, wl in enumerate(workloads):

            # create a folder for each app 
            outputPathApp = outputPath + "/" + wl[0] 
            os.makedirs(os.path.abspath(outputPathApp), exist_ok=True)

            for ways in range(1,21):
 
                wl_in_path = args.inputdir + "/" + str(ways) + "ways/outputFilesMean/" + wl[0] + ".csv"
                #print(wl_name) 

                dfWay = pd.read_table(wl_in_path, sep=",")  
                dfWay = dfWay[["interval","ipnc:mean","ipnc:std","ev0:mean","ev0:std","l3_kbytes_occ:mean","l3_kbytes_occ:std"]]          
      

                # calculate confidence interval of IPC
                dfWay["ipnc:std"] =  Z * ( dfWay["ipnc:std"] / SQRT_NUM_REPS_APP )
 
                # calculate hits/storage
                relErrEV0 = dfWay["ev0:std"] / dfWay["ev0:mean"]
                relErrL3OCCUP = dfWay["l3_kbytes_occ:std"] / dfWay["l3_kbytes_occ:mean"]
                relErrDiv = np.sqrt((relErrEV0*relErrEV0) + (relErrL3OCCUP*relErrL3OCCUP))

                dfWay["ev0:mean"] = dfWay["ev0:mean"] / dfWay["l3_kbytes_occ:mean"]
                dfWay["ev0:std"] = relErrDiv * dfWay["ev0:mean"]
                dfWay["ev0:std"] =  Z * ( dfWay["ev0:std"] / SQRT_NUM_REPS_APP_2 )

                # rename columns 
                dfWay = dfWay[["interval","ipnc:mean","ipnc:std","ev0:mean","ev0:std"]]
                dfWay = dfWay.rename(columns={'ipnc:mean': 'IPC:mean', 'ipnc:std': 'IPC:ci', 'ev0:mean': 'hits/storage:mean', 'ev0:std': 'hits/storage:ci'})   
               
                # save table with number of ways in x-axis
                dfWay = dfWay.set_index(['interval'])
                print("dfWay with intervals")
                print(dfWay)
                outputPathAppWay = outputPathApp + "/" + wl[0] + "-" + str(ways) + "ways-intervalDataTable.csv"
                print(outputPathAppWay)
                dfWay.to_csv(outputPathAppWay, sep=',')


# El main es crida des d'ací
if __name__ == "__main__":
    main()

