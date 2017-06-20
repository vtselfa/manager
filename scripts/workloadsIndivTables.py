import argparse
import numpy as np
import os
import pandas as pd
import re
import scipy.stats
import sys
import yaml


def main():
    parser = argparse.ArgumentParser(description='Process results of workloads by intervals.')
    parser.add_argument('-w', '--workloads', required=True, help='.yaml file where the list of workloads is found.')
    parser.add_argument('-od', '--outputdir', default='./output', help='Directory where output files will be placed')
    parser.add_argument('-id', '--inputdir', default='./data', help='Directory where input are found')
    parser.add_argument('-f', '--functions', action='append', default=[], help='Functions to perform to generate tables. Options: ipc,hits,occup.')
    parser.add_argument('-p', '--policies', action='append', default=[], help='Policies we want to show results of. Options: np,hg.')    
    parser.add_argument('-dc', '--dataCollection', action='append', default=[], help='How data must be collected. Options: Total,Inteval.')

    args = parser.parse_args()

    print(args.functions)

    for func in args.functions:
        assert( (func == "ipc") | (func == "hits") | (func == "occup") ) 

    for policy in args.policies:
        assert( (policy == "np") | (policy == "hg") )
    
    for datac in args.dataCollection:
        assert( (datac == "Total") | (datac =="Interval") )

    print(args.workloads)

    with open(args.workloads, 'r') as f:
        workloads = yaml.load(f)
 
    os.makedirs(os.path.abspath(args.outputdir), exist_ok=True)

    #type of file from which obtain the data (tot or fin)
    fileType = "tot"

    for datac in args.dataCollection:

        outputPath= os.path.abspath(args.outputdir)
        os.makedirs(os.path.abspath(outputPath + "/" + datac), exist_ok=True)
        print(outputPath)

        for wl_id, wl in enumerate(workloads):

            dfTotal = pd.DataFrame()

            for policy in args.policies:

                #name of the file with raw data
                wl_show_name = "-".join(wl)
                wl_name = args.inputdir + policy + datac + "/outputFilesMean/" + wl_show_name + "_0_" + fileType + ".csv"
                print(wl_name)

                #create dataframe from raw data
                df = pd.read_table(wl_name, sep=",")
        
                dfsub = df[["app","ipnc","l3_kbytes_occ","ev0"]]
                dfsub = dfsub.rename(columns={'ipnc': 'IPC-'+policy, 'ev0': 'HitsL3-'+policy, 'l3_kbytes_occ' : 'OccupL3-'+policy})

                if dfTotal.size == 0:
                    dfTotal = dfsub
                    print("dfTotal is empty!")
                    print(dfTotal)
                else:
                    dfTotal = pd.merge(dfTotal,dfsub,how='left',on='app')

            print(dfTotal)
            dfTotal = dfTotal.set_index('app') 
            filepath = outputPath + "/results-" + fileType + "_" + "-".join(wl) + "_" + datac + ".csv"
            print(filepath)

            dfTotal.to_csv(filepath, sep=',')


# El main es crida des d'ací
if __name__ == "__main__":
    main()

