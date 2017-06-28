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
    NUM_REPS_APP_2 = 6
    SQRT_NUM_REPS_APP_2 = np.sqrt(NUM_REPS_APP_2)


    #print(args.functions)

    for policy in args.policies:
        assert( (policy == "np") | (policy == "hg") )
    
    for datac in args.dataCollection:
        assert( (datac == "Total") | (datac =="Interval") )

    #print(args.workloads)

    with open(args.workloads, 'r') as f:
        workloads = yaml.load(f)

    # type of file from which obtain the data (tot or fin)
    # fin = stats taken at the endo of the execution of each app
    # tot = stats taken at the end of the execution of the last app
    fileType = "fin"    

    outputPath= os.path.abspath(args.outputdir)
    os.makedirs(os.path.abspath(outputPath), exist_ok=True)
    #print(outputPath)
	
    for datac in args.dataCollection:

        for policy in args.policies:

            outputPathPolicyDataC = outputPath+ "/" + policy + datac
            outputPathPolicyDataC = os.path.abspath(outputPathPolicyDataC)
            os.makedirs(os.path.abspath(outputPathPolicyDataC), exist_ok=True)
            #print(outputPathWorkload)
 
            for wl_id, wl in enumerate(workloads):
                
                #name of the file with raw data
                wl_show_name = "-".join(wl) 
                print(wl_show_name)
             
                outputPathWorkload = outputPathPolicyDataC + '/' + wl_show_name
                os.makedirs(os.path.abspath(outputPathWorkload), exist_ok=True)

               
                wl_name = args.inputdir + policy + datac + "/outputFilesMean/" + wl_show_name + "-" + fileType + ".csv"
                #print(wl_name) 
                      
	        #create dataframe from raw data 
                df = pd.read_table(wl_name, sep=",")
                dfsub = df[["app","ipnc:mean","ipnc:std","ev0:mean","ev0:std"]]
                dfsub = dfsub.set_index(['app'])
                groups = dfsub.groupby(level=[0])

                # dataframe for individual execution    
                dfApp = pd.DataFrame()
                coreIndex = 0
                
                for app, df in groups:
                   
                   wl_app = args.inputdir + policy  + datac + "/outputFilesMean/" + wl_show_name + "/" +  app + "." + str(coreIndex) + ".csv"
                   print(wl_app)
                   dfApp = pd.read_table(wl_app, sep=",")  
                   dfApp = dfApp[["ipnc:mean","ipnc:std","ev0:mean","ev0:std","l3_kbytes_occ:mean","l3_kbytes_occ:std"]] 
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
         
                   dfApp = dfApp[["numberOfWays:mean","IPC:mean","IPC:ci","hits/storage:mean","hits/storage:ci"]]
                   dfApp = dfApp.set_index(['numberOfWays:mean'])     
                   print("dfApp AFTER performing calculations")
                   print(dfApp)
               
                   # save table 
                   outputPathAppFile = outputPathWorkload + "/" + app + "-numWaysDataTable-" + fileType + ".csv"
                   print(outputPathAppFile)
                   dfApp.to_csv(outputPathAppFile, sep=',')

                   coreIndex = int(coreIndex) + 1



# El main es crida des d'ací
if __name__ == "__main__":
    main()

