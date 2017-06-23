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
    Z = 1.96  # value of z at a 95% c.i.
    NUM_REPS_APP = 3  # num of values used to calculate the mean value
    SQRT_NUM_REPS_APP = np.sqrt(NUM_REPS_APP)
    NUM_REPS_W = 24
    SQRT_NUM_REPS_W = np.sqrt(NUM_REPS_W)
    NUM_REPS_W_2 = 48
    SQRT_NUM_REPS_W_2 = np.sqrt(NUM_REPS_W_2)



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
            columns = ['Workload', 'IPC:mean', 'IPC:ci', 'hitsL3:mean', 'hitsL3:ci', 'ANTT:mean', 'ANTT:ci', 'STP:mean', 'STP:ci']
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
               
                   print("dfsub before calculating confidence intervals")
                   print(dfsub)

                   # calculate confidence intervals from stds
                   IPCSTD = dfsub.loc[app,'IPC:std']
                   hitsL3STD = dfsub.loc[app,'hitsL3:std']    
                   progressSTD = dfsub.loc[app,'progress:std']
                   slowdownSTD = dfsub.loc[app,'slowdown:std']

                   #print("IPC before calculating confidence interval")
                   #print(dfsub.loc[app,'IPC:std'])
                   dfsub.loc[app,'IPC:std'] = Z * ( dfsub.loc[app,'IPC:std'] / SQRT_NUM_REPS_APP )
                   #print("IPC after calculating confidence interval")
                   #print(dfsub.loc[app,'IPC:std'])
                   dfsub.loc[app,'hitsL3:std'] = Z * ( dfsub.loc[app,'hitsL3:std'] / SQRT_NUM_REPS_APP )
                   dfsub.loc[app,'progress:std'] = Z * ( dfsub.loc[app,'progress:std'] / SQRT_NUM_REPS_APP )
                   dfsub.loc[app,'slowdown:std'] = Z * ( dfsub.loc[app,'slowdown:std'] / SQRT_NUM_REPS_APP )

                   print("dfsub after calculating confidence intervals")
                   print(dfsub)



                # save table
                dfsub = dfsub.rename(columns={'IPC:std': 'IPC:ci', 'hitsL3:std': 'hitsL3:ci', 'progress:std': 'progress:ci', 'slowdown:std': 'slowdown:ci'})
                outputPathWorkloadFile = outputPathWorkload + "/slowdown-table-" + wl_show_name + "-" + fileType + ".csv"
                #print("outputPathWorkloadFile") 
                #print(outputPathWorkloadFile)
                dfsub.to_csv(outputPathWorkloadFile, sep=',')
 
                # add total value of progress, mean value of slowdown to total table 
                dfTotal.ix[workloadIndex, 'STP:mean'] = dfsub["progress:mean"].sum()
                dfTotal.ix[workloadIndex, 'STP:ci'] = np.sqrt((dfsub.loc[app,'progress:ci'] * dfsub.loc[app,'progress:ci']).sum()) 
                
                dfTotal.ix[workloadIndex, 'ANTT:mean'] = dfsub["slowdown:mean"].mean()
                dfTotal.ix[workloadIndex, 'ANTT:ci'] = np.sqrt((dfsub.loc[app,'slowdown:ci'] * dfsub.loc[app,'slowdown:ci']).sum()) * (1 / dfsub["slowdown:mean"].count())

                dfTotal.ix[workloadIndex, 'IPC:mean'] = dfsub["IPC:mean"].sum()
                dfTotal.ix[workloadIndex, 'IPC:ci'] = np.sqrt((dfsub.loc[app,'IPC:ci'] * dfsub.loc[app,'IPC:ci']).sum())

                dfTotal.ix[workloadIndex, 'hitsL3:mean'] = dfsub["hitsL3:mean"].sum()
                dfTotal.ix[workloadIndex, 'hitsL3:ci'] = np.sqrt((dfsub.loc[app,'hitsL3:ci'] * dfsub.loc[app,'hitsL3:ci']).sum())

                #print("dfTotal after adding values")
                #print(dfTotal) 
 
                workloadIndex = workloadIndex + 1
                 


            filepath = outputPath + "/totalERRTable-" + fileType + "-" + policy + datac + ".csv"
            print(filepath)

            dfTotal.to_csv(filepath, sep=',')


# El main es crida des d'ací
if __name__ == "__main__":
    main()

