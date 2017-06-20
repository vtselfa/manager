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
    parser.add_argument('-dc', '--dataCollection', action='append', default=[], help='How data must be collected. Options: Total,Inteval.')

    args = parser.parse_args()

    

    #print(args.functions)

    for policy in args.policies:
        assert( (policy == "np") | (policy == "hg") )
    
    for datac in args.dataCollection:
        assert( (datac == "Total") | (datac =="Interval") )

    #print(args.workloads)

    with open(args.workloads, 'r') as f:
        workloads = yaml.load(f)

    #type of file from which obtain the data (tot or fin)
    fileType = "fin"    

    outputPath= os.path.abspath(args.outputdir)
    os.makedirs(os.path.abspath(outputPath), exist_ok=True)
    #print(outputPath)
	
    for datac in args.dataCollection:

        for policy in args.policies:

            outputPathWorkload = outputPath+ "/" + policy + datac
            outputPathWorkload = os.path.abspath(outputPathWorkload)
            os.makedirs(os.path.abspath(outputPathWorkload), exist_ok=True)
            #print(outputPathWorkload)

            # dataframe for policy execution with total slowdown for each workload       
            columns = ['Workload', 'IPC:mean', 'IPC:std', 'hitsL3:mean', 'hitsL3:std', 'ANTT:mean', 'ANTT:std', 'STP:mean', 'STP:std']
            index = range(1, len(workloads) + 1)
            dfTotal = pd.DataFrame(columns=columns, index = index)
            #dfTotal = dfTotal.set_index(['Workload'])
            workloadsList = []
            print("dfTotal 0")
            print(dfTotal)

            workloadIndex = 1
 
            for wl_id, wl in enumerate(workloads):
                
                coreIndex = -1;
                 
                #name of the file with raw data
                wl_show_name = "-".join(wl) 
                print(wl_show_name)
                dfTotal.ix[workloadIndex,'Workload'] = wl_show_name
                workloadsList.append(wl_show_name)
                print("dfTotal after workload")
                print(dfTotal)               

 
                wl_name = args.inputdir + policy + datac + "/outputFilesMean/" + wl_show_name + "-" + fileType + ".csv"
                #print(wl_name) 
                      
	        #create dataframe from raw data 
                df = pd.read_table(wl_name, sep=",")
                dfsub = df[["app","ipnc:mean","ipnc:std","ev0:mean","ev0:std"]]
                dfsub = dfsub.rename(columns={'ipnc:mean': 'IPC:mean', 'ipnc:std': 'IPC:std', 'ev0:mean': 'hitsL3:mean', 'ev0:std': 'hitsL3:std'})                    
                dfsub['progress:mean'] = dfsub['IPC:mean']
                dfsub['progress:std'] = dfsub['IPC:std']
                dfsub['slowdown:mean'] = dfsub['IPC:mean']
                dfsub['slowdown:std'] = dfsub['IPC:std']
                dfsub = dfsub.set_index(['app'])
                groups = dfsub.groupby(level=[0])
                #print(dfsub)

                # dataframe for individual execution    
                dfIndiv = pd.DataFrame()
                
                # divide each IPC by IPC of individual execution
                for app, df in groups:
                   words = app.split('_')
                   appName = words[1]
                   #print(appName)
                   
                   wl_indiv = args.inputdir + "/npIndiv" + datac + "/outputFilesMean/" + appName + "-" + fileType + ".csv"
                   #print(wl_indiv)
                   dfIndiv = pd.read_table(wl_indiv, sep=",")  
                   dfIndiv = dfIndiv[["app","ipnc:mean","ipnc:std"]] 
                   dfIndiv = dfIndiv.set_index(['app'])     
                   #print("dfIndiv")
                   #print(dfIndiv)
 
                   words.pop(0)
                   coreIndex = int(coreIndex) + 1
                   appIndiv = "00"
                   for w in words:
                       appIndiv = appIndiv + "_" + w
                   #appIndiv = "0" + appIndiv

                   # calculate values of progress and slowdown 
                   dfsub.loc[app,'progress:mean'] = dfsub.loc[app,'IPC:mean'] / dfIndiv.loc[appIndiv,'ipnc:mean']
                   dfsub.loc[app,'slowdown:mean'] = dfIndiv.loc[appIndiv,'ipnc:mean'] / dfsub.loc[app,'IPC:mean']

                   # calculation of stds
                   #  a) calculate relative errors 
                   relErrIPCcomp = dfsub.loc[app,'IPC:std'] / dfsub.loc[app,'IPC:mean']
                   relErrIPCindiv = dfIndiv.loc[appIndiv,'ipnc:std'] / dfIndiv.loc[appIndiv,'ipnc:mean']

                   #  b) calculate resultant error of division 
                   relErrDiv = np.sqrt((relErrIPCcomp*relErrIPCcomp) + (relErrIPCindiv*relErrIPCindiv))

                   #  c) calculate stds of results
                   dfsub.loc[app,'progress:std'] = relErrDiv * dfsub.loc[app,'progress:mean']
                   dfsub.loc[app,'slowdown:std'] = relErrDiv * dfsub.loc[app,'slowdown:mean']
               
                   #print("dfsub after division")
                   #print(dfsub)


                # save table
                outputPathWorkloadFile = outputPathWorkload + "/slowdown-table-" + wl_show_name + "-" + fileType + ".csv"
                print("outputPathWorkloadFile") 
                print(outputPathWorkloadFile)
                dfsub.to_csv(outputPathWorkloadFile, sep=',')
 
                # add total value of progress, mean value of slowdown to total table 
                dfsub = dfsub.rename(columns={'progress:mean': 'STP:mean', 'progress:std': 'STP:std','slowdown:mean': 'ANTT:mean', 'slowdown:std': 'ANTT:std'})


                dfTotal.ix[workloadIndex, 'STP:mean'] = dfsub["STP:mean"].sum()
                dfTotal.ix[workloadIndex, 'STP:std'] = np.sqrt((dfsub["STP:std"] * dfsub["STP:std"]).sum()) 
                dfTotal.ix[workloadIndex, 'ANTT:mean'] = dfsub["ANTT:mean"].mean()
                dfTotal.ix[workloadIndex, 'ANTT:std'] = np.sqrt((dfsub["ANTT:std"] * dfsub["ANTT:std"]).sum()) * (1 / dfsub["ANTT:mean"].count())
 
                dfTotal.ix[workloadIndex, 'IPC:mean'] = dfsub["IPC:mean"].sum()
                dfTotal.ix[workloadIndex, 'IPC:std'] = np.sqrt((dfsub["IPC:std"] * dfsub["IPC:std"]).sum())
                dfTotal.ix[workloadIndex, 'hitsL3:mean'] = dfsub["hitsL3:mean"].sum()
                dfTotal.ix[workloadIndex, 'hitsL3:std'] = np.sqrt((dfsub["hitsL3:std"] * dfsub["hitsL3:std"]).sum())

                print("dfTotal after adding values")
                print(dfTotal) 

                workloadIndex = workloadIndex + 1
                 


            filepath = outputPath + "/slowdownTable-" + policy + datac + "-" + fileType + ".csv"
            print(filepath)

            dfTotal.to_csv(filepath, sep=',')


# El main es crida des d'ací
if __name__ == "__main__":
    main()

