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

        for wl_id, wl in enumerate(workloads):

            # create a df for the app    
            columns = ['num-ways', 'IPC:mean', 'IPC:ci', 'hits/storage:mean', 'hits/storage:ci']
            index = range(1, 21)
            dfApp = pd.DataFrame(columns=columns, index = index)

            for ways in range(1,21):

                dfApp.ix[ways, 'num-ways'] = ways
 
                wl_in_path = args.inputdir + "/" + str(ways) + "ways/outputFilesMean/" + wl[0] + ".csv"
                #print(wl_name) 
                
                dfWay = pd.read_table(wl_in_path, sep=",")  
                dfWay = dfWay[["ipnc:mean","ipnc:std","ev0:mean","ev0:std","l3_kbytes_occ:mean","l3_kbytes_occ:std"]] 

                dfApp.ix[ways, 'IPC:mean'] = dfWay["ipnc:mean"].mean()
  
                # calculate confidence interval of IPC
                IPCstd = dfWay["ipnc:std"].mean()
                dfApp.ix[ways, 'IPC:ci'] = Z * ( IPCstd / SQRT_NUM_REPS_APP )
 
                # calculate hits/storage
                relErrEV0 = (dfWay["ev0:std"] / dfWay["ev0:mean"]).mean()
                relErrL3OCCUP = (dfWay["l3_kbytes_occ:std"] / dfWay["l3_kbytes_occ:mean"]).mean()
                relErrDiv = np.sqrt((relErrEV0*relErrEV0) + (relErrL3OCCUP*relErrL3OCCUP))

                dfWay["ev0:mean"] = dfWay["ev0:mean"] / dfWay["l3_kbytes_occ:mean"]
                dfWay["ev0:std"] = relErrDiv * dfWay["ev0:mean"]
                dfWay["ev0:std"] =  Z * ( dfWay["ev0:std"] / SQRT_NUM_REPS_APP_2 )

                dfApp.ix[ways, 'hits/storage:mean'] = dfWay["ev0:mean"].mean()
                dfApp.ix[ways, 'hits/storage:ci'] = dfWay["ev0:std"].mean()

               
            # save table with number of ways in x-axis
            dfApp = dfApp.set_index(['num-ways'])
            print("dfApp with number of ways")
            print(dfApp)
            outputPathAppFile = outputPath + "/" + wl[0] + "-numWaysDataTable.csv"
            print(outputPathAppFile)
            dfApp.to_csv(outputPathAppFile, sep=',')


# El main es crida des d'ací
if __name__ == "__main__":
    main()

