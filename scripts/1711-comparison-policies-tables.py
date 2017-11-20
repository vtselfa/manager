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
    parser.add_argument('-p', '--policies', action='append', default=[], help='Policies we want to show results of. Options: np,hg.')

    args = parser.parse_args()

    print(args.workloads)

    with open(args.workloads, 'r') as f:
        workloads = yaml.load(f)

    outputPath= os.path.abspath(args.outputdir)
    os.makedirs(os.path.abspath(outputPath), exist_ok=True)

    for policy in args.policies:

        # create Data Frame with total values for each workload of the policy
        columns = ['Workload_ID','Workload',"IPC:mean","MPKIL3:mean","STP:mean","ANTT:mean","Unfairness:mean","Tt:mean"]
        index = range(0, len(args.workloads))
        dfPolicy = pd.DataFrame(columns=columns, index = index)
        dfPolicy['Workload_ID'] = range(1, len(args.workloads)+1)

        numW = 0
        for wl_id, wl in enumerate(workloads):

            #name of the file with raw data
            wl_show_name = "-".join(wl)

            dfPolicy.ix[numW,'Workload'] = wl_show_name

            wl_name = args.inputdir +  "/" + wl_show_name + "-totalDataTable.csv"
            #print(wl_name)

	    #create dataframe from raw data
            dfworkload = pd.read_table(wl_name, sep=",")
            dfworkload = dfworkload[["policy","IPC:mean","MPKIL3:mean","STP:mean","ANTT:mean","Unfairness:mean","Tt:mean"]]

            dfworkload = dfworkload.set_index(['policy'])

            dfPolicy.ix[numW,'IPC:mean'] = dfworkload.loc[policy]['IPC:mean']
            dfPolicy.ix[numW,'MPKIL3:mean'] = dfworkload.loc[policy]['MPKIL3:mean']
            dfPolicy.ix[numW,'STP:mean'] = dfworkload.loc[policy]['STP:mean']
            dfPolicy.ix[numW,'ANTT:mean'] = dfworkload.loc[policy]['ANTT:mean']
            dfPolicy.ix[numW,'Unfairness:mean'] = dfworkload.loc[policy]['Unfairness:mean']
            dfPolicy.ix[numW,'Tt:mean'] = dfworkload.loc[policy]['Tt:mean']

            numW = int(numW) + 1

        # save table
        dfPolicy = dfPolicy.set_index(['Workload_ID'])
        outputPathPolicy = outputPath + "/" + policy + "-workloads-totals.csv"
        print(outputPathPolicy)
        dfPolicy.to_csv(outputPathPolicy, sep=',')



# El main es crida des d'ací
if __name__ == "__main__":
    main()

