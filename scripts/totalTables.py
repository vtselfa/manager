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
    parser.add_argument('-f', '--functions', action='append', default=[], help='Functions to perform to generate tables. Options: ipc,hits')
    parser.add_argument('-p', '--policies', action='append', default=[], help='Policies we want to show results of. Options: np,hg.')    
    parser.add_argument('-dc', '--dataCollection', action='append', default=[], help='How data must be collected. Options: Total,Inteval.')

    args = parser.parse_args()

    

    #print(args.functions)

    for func in args.functions:
        assert( (func == "ipc") | (func == "hits") ) 

    for policy in args.policies:
        assert( (policy == "np") | (policy == "hg") )
    
    for datac in args.dataCollection:
        assert( (datac == "Total") | (datac =="Interval") )

    #print(args.workloads)

    with open(args.workloads, 'r') as f:
        workloads = yaml.load(f)
 
    os.makedirs(os.path.abspath(args.outputdir), exist_ok=True)

    #type of file from which obtain the data (tot or fin)
    fileType = "fin"    
	
    for datac in args.dataCollection:

        outputPath= os.path.abspath(args.outputdir)
        os.makedirs(os.path.abspath(outputPath), exist_ok=True)
        print(outputPath)

        for policy in args.policies:
       
            dfTotal = pd.DataFrame()
            dfTotal['Index'] = range(1, len(dfTotal) + 1)             
 
            workloadsList = []
            dfTotal['Workload'] = workloadsList 
   
            for wl_id, wl in enumerate(workloads):
                 
                #name of the file with raw data
                wl_show_name = "-".join(wl) 

                workloadsList.append(wl_show_name)
      
                dfaux = pd.DataFrame()
                wl_name = args.inputdir + policy + datac + "/outputFilesMean/" + wl_show_name + "-" + fileType + ".csv"
                print(wl_name) 
                      
                #create dataframe from raw data 
                df = pd.read_table(wl_name, sep=",")
                dfsub = df[["ipnc:mean","ipnc:std","ev0:mean","ev0:std"]]
                dfsub = dfsub.rename(columns={'ipnc:mean': 'IPC', 'ev0:mean': 'hitsL3', 'ipnc:std': 'IPC:std', 'ev0:std': 'hitsL3:std'})

                print(dfsub)
                dfsub['IPC:std'] = dfsub['IPC:std'] * dfsub['IPC:std']
                #print(dfsub['IPC:std'])            

                #print(dfsub['hitsL3:std'])
                dfsub['hitsL3:std'] = dfsub['hitsL3:std'] * dfsub['hitsL3:std']
                #print(dfsub['hitsL3:std'])

                print(dfsub)

                dfaux = dfsub.sum()
                print(dfaux)
                
                dfaux['IPC:std'] = np.sqrt(dfaux['IPC:std'])
                dfaux['hitsL3:std'] = np.sqrt(dfaux['hitsL3:std'])
                print(dfaux)

                dfTotal = dfTotal.append(dfaux, ignore_index=True)

            dfTotal['Workload'] = workloadsList
            dfTotal['Index'] = range(1, len(dfTotal) + 1)
            dfFinal = dfTotal.set_index('Index')
           
            print(dfFinal)            

            filepath = outputPath + "/totalTable-" + policy + datac + "-" + fileType + ".csv"
            print(filepath)

            dfFinal.to_csv(filepath, sep=',')


# El main es crida des d'ací
if __name__ == "__main__":
    main()

