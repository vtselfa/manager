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
    parser.add_argument('-f', '--functions', action='append', default=[], help='Functions to perform to generate tables. Options: ipc,hits,hitsOccup.')
    parser.add_argument('-p', '--policies', action='append', default=[], help='Policies we want to show results of. Options: np,hg.')    
    parser.add_argument('-dc', '--dataCollection', action='append', default=[], help='How data must be collected. Options: Total,Inteval.')

    args = parser.parse_args()

    #print(args.functions)

    for func in args.functions:
        assert( (func == "ipc") | (func == "hits") | (func == "hitsOccup") ) 

    for policy in args.policies:
        assert( (policy == "np") | (policy == "hg") )
    
    for datac in args.dataCollection:
        assert( (datac == "Total") | (datac =="Interval") )

    #print(args.workloads)

    with open(args.workloads, 'r') as f:
        workloads = yaml.load(f)
 
    os.makedirs(os.path.abspath(args.outputdir), exist_ok=True)

    result = pd.DataFrame()
    for wl_id, wl in enumerate(workloads):
	
        for datac in args.dataCollection:

            #create directory for each workload 
            outputPath= os.path.abspath(args.outputdir) + "/" + datac + "/" + "-".join(wl)
            os.makedirs(os.path.abspath(outputPath), exist_ok=True)
            print(outputPath)

            

            for policy in args.policies: 

                #name of the file with raw data 
                wl_name = args.inputdir + policy + datac + "/data/" + "-".join(wl) + "_0.csv"
                print(wl_name)
                    
                #create dataframe from raw data 
                df = pd.read_table(wl_name, sep=",")
        
                dfsub = df[["interval","app","core","compl","ipnc","l3_kbytes_occ","ev0"]]
                dfsub = dfsub.rename(columns={'ipnc': 'IPC', 'ev0': 'hitsL3'})
                dfsub = dfsub.set_index(['interval', 'app', 'core'])
   
                completed = dfsub['compl'] <= 1
                dfsub = dfsub[completed]
                   
                groups = dfsub.groupby(level=[1,2])

                for (app, core), df in groups: 
                    for func in args.functions:
                        name = func + "Tables(df, app, outputPath, policy)"
                        print(name)
                        eval(name)


#generation of tables per function

def ipcTables(df,app,outputPath,policy):
    #print(app)
    #print(df.columns)
    df = df[["IPC"]]
    df.index = df.index.droplevel(1) #Drop app name
    df.index = df.index.droplevel(1) #Drop core number
    #print(df)

    filepath = outputPath + "/" + app + "_IPC" + policy + ".csv"
    print(filepath)

    df.to_csv(filepath, sep=',')


def hitsTables(df,app,outputPath,policy):
    df = df[["hitsL3"]]
    df.index = df.index.droplevel(1) #Drop app name
    df.index = df.index.droplevel(1) #Drop core number
    #print(df)

    filepath = outputPath + "/" + app + "_hitsL3" + policy + ".csv"
    print(filepath)

    df.to_csv(filepath, sep=',')



def hitsOccupTables(df,app,outputPath,policy):
    dfsub = df[["hitsL3"]]
    dfsub = dfsub[["hitsL3"]].div(df.l3_kbytes_occ, axis=0)
    dfsub = dfsub.rename(columns={'hitsL3': 'hitsperOccupL3(KB)'})
    dfsub.index = dfsub.index.droplevel(1) #Drop app name
    dfsub.index = dfsub.index.droplevel(1) #Drop core number
    #print(dfsub)

    filepath = outputPath + "/" + app + "_hitsperOccupL3" + policy + ".csv"
    print(filepath)

    dfsub.to_csv(filepath, sep=',')


# El main es crida des d'ací
if __name__ == "__main__":
    main() 
