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
    parser.add_argument('-p', '--policies', action='append', default=[], help='Policies we want to show results of.')

    args = parser.parse_args()

    print(args.workloads)

    with open(args.workloads, 'r') as f:
        workloads = yaml.load(f)

    outputPath= os.path.abspath(args.outputdir)

    # create Data Frame with value of Turn Around time for each workload in each policy
    columns = ['Workload_ID','Workload']

    for p in args.policies:
    	columns.append(p)

    print("columns : ",columns)
    print("Num workloads: ",len(args.workloads))
    #index = range(0, len(args.workloads))
    index = range(0, 7)
    df = pd.DataFrame(columns=columns, index = index)
    #df['Workload_ID'] = range(1, len(args.workloads)+1)
    df['Workload_ID'] = range(1, 8)


    for policy in args.policies:

        numW = 0
        for wl_id, wl in enumerate(workloads):

            #name of the file with raw data
            wl_show_name = "-".join(wl)

            df.ix[numW,'Workload'] = wl_show_name

            wl_name = args.inputdir + "/" + policy +  "/data-agg/" + wl_show_name + "_tot.csv"
            #print(wl_name)

            #create dataframe from raw data
            dfworkload = pd.read_table(wl_name, sep=",")

            df.ix[numW,policy] = dfworkload['interval:mean'].max()

            numW = int(numW) + 1

    # save table
    df = df.set_index(['Workload_ID'])
    outputPathPolicy = outputPath + "/" + policy + "-tt-table.csv"
    print(df)
    df.to_csv(outputPathPolicy, sep=',')


# El main es crida des d'ací
if __name__ == "__main__":
    main()
