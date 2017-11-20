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
    parser.add_argument('-lw', '--listways', default=[], action='append', help='List of tumples with (number of ways given to crancbmk, nmber of ways given to the rest of apps). Example: (12,8)')
    args = parser.parse_args()
    Z = 1.96  # value of z at a 95% c.i.
    NUM_REPS_APP = 3  # num of values used to calculate the mean value
    SQRT_NUM_REPS_APP = np.sqrt(NUM_REPS_APP)
    NUM_REPS_APP_2 = 6
    SQRT_NUM_REPS_APP_2 = np.sqrt(NUM_REPS_APP_2)
    numApps = 8

    ## EVENTS USED ##
    ev0 = "MEM_LOAD_UOPS_RETIRED.L3_HIT"
    ev1 = "MEM_LOAD_UOPS_RETIRED.L3_MISS"
    ev2 = "CYCLE_ACTIVITY.STALLS_TOTAL"
    ev3 = "CYCLE_ACTIVITY.STALLS_LDM_PENDING"


    with open(args.workloads, 'r') as f:
        workloads = yaml.load(f)

    outputPath = os.path.abspath(args.outputdir)
    os.makedirs(os.path.abspath(outputPath), exist_ok=True)

    for wl_id, wl in enumerate(workloads):

            list_ways = args.listways

            wl_show_name = "-".join(wl)

            #create total dataframe
            columns = ['configuration', 'IPC:mean', 'IPC:ci', 'IPC_prediction:mean', 'median_rel_err_IPC', 'MPKIL3:mean', 'MPKIL3:ci','STP:mean', 'STP:ci', 'ANTT:mean', 'ANTT:ci', 'Unfairness:mean', 'Unfairness:ci', 'Tt:mean', 'Tt:ci']
            index = range(0, len(list_ways))
            dfTotal = pd.DataFrame(columns=columns, index = index)

            numConfig = 0
            for way_id, way in enumerate(list_ways):
                aux = way.split('(')
                aux = aux[1].split(')')
                aux = aux[0].split(',')
                waysCR = aux[0]
                waysO = aux[1]


                # total values of execution
                dfTotal.ix[numConfig,'configuration'] = str(waysCR) + "cr" + str(waysO) + "others"

                wl_in_path = args.inputdir + "/" + str(waysCR) + "cr" + str(waysO) + "others/outputFilesMean/" + wl_show_name + "-fin.csv"

                print(wl_in_path)

                dfaux = pd.read_table(wl_in_path, sep=",")

                dfaux = dfaux[["instructions:mean","instructions:std","ipnc:mean","ipnc:std",ev1+":mean",ev1+":std"]]


                dfTotal.ix[numConfig,'IPC:mean'] = dfaux["ipnc:mean"].sum()
                dfTotal.ix[numConfig,'IPC:ci'] = Z * ((np.sqrt((dfaux["ipnc:std"]*dfaux["ipnc:std"]).sum())) / SQRT_NUM_REPS_APP)

                #print("dfTotal with IPC")
                #print(dfTotal)

                relErrIns = (dfaux["instructions:std"] / dfaux["instructions:mean"] )**2
                relErrEv1 = (dfaux[ev1+":std"] / dfaux[ev1+":mean"] )**2
                relErr = np.sqrt(relErrIns + relErrEv1)
                dfaux[ev1+":mean"] = dfaux[ev1+":mean"] / (dfaux["instructions:mean"] / 1000)
                dfaux[ev1+":std"] = dfaux[ev1+":mean"] * relErr
                dfaux[ev1+":std"] = Z * ( dfaux[ev1+":std"] / SQRT_NUM_REPS_APP )

                dfTotal.ix[numConfig,'MPKIL3:mean'] = dfaux[ev1+":mean"].sum()
                dfTotal.ix[numConfig,'MPKIL3:ci'] =np.sqrt((dfaux[ev1+":std"]*dfaux[ev1+":std"]).sum())

                #print("dfTotal with MPKIL3")
                #print(dfTotal)

                # dataframe for interval table
                wl_in_path = args.inputdir + "/" + str(waysCR) + "cr" + str(waysO) + "others/outputFilesMean/" + wl_show_name + ".csv"
                print(wl_in_path)

                dfWay = pd.read_table(wl_in_path, sep=",")
                dfWay = dfWay[["interval","app","instructions:mean","instructions:std","ipnc:mean","ipnc:std","ipnc_prediction:mean",ev1+":mean",ev1+":std","l3_kbytes_occ:mean","l3_kbytes_occ:std"]]
                #dfWay = dfWay.set_index(['interval'])
                #groups = dfWay.groupby(level=[0])

                # calculate confidence interval of IPC
                dfWay["ipnc:std"] =  Z * ( dfWay["ipnc:std"] / SQRT_NUM_REPS_APP )

                # calculate l3_kbytes_occ in mbytes
                dfWay["l3_kbytes_occ:mean"] = dfWay["l3_kbytes_occ:mean"] / 1024
                dfWay["l3_kbytes_occ:std"] = dfWay["l3_kbytes_occ:std"] / 1024
                # calculate confidence interval of l3_kbytes_occ
                dfWay["l3_kbytes_occ:std"] = Z * ( dfWay["l3_kbytes_occ:std"] / SQRT_NUM_REPS_APP )

                # rename columns
                dfWay = dfWay.rename(columns={'ipnc:mean': 'IPC:mean', 'ipnc:std': 'IPC:ci', 'ipnc_prediction:mean' : 'IPC_prediction:mean', 'l3_kbytes_occ:mean': 'l3_Mbytes_occ:mean', 'l3_kbytes_occ:std': 'l3_Mbytes_occ:ci'})

                # calculate MPKI
                relErrIns = (dfWay["instructions:std"] / dfWay["instructions:mean"] )**2
                relErrEv1 = (dfWay[ev1+":std"] / dfWay[ev1+":mean"] )**2
                relErr = np.sqrt(relErrIns + relErrEv1)

                dfWay[ev1+":mean"] = dfWay[ev1+":mean"] / (dfWay["instructions:mean"] / 1000)
                dfWay[ev1+":std"] = dfWay[ev1+":mean"] * relErr
                dfWay[ev1+":std"] = Z * ( dfWay[ev1+":std"] / SQRT_NUM_REPS_APP )
                dfWay = dfWay.rename(columns={ev1+':mean': 'MPKIL3:mean', ev1+':std': 'MPKIL3:ci'})

                # GENERATE Interval tables app per workload
                dfWay = dfWay[["interval","app","IPC:mean","IPC:ci","IPC_prediction:mean","l3_Mbytes_occ:mean","l3_Mbytes_occ:ci","MPKIL3:mean","MPKIL3:ci"]]

                dfWayAux = dfWay
                dfWay = dfWay.set_index(['interval','app'])
                groups = dfWay.groupby(level=[1])

                # calculate relative error

                dfWay["rel_err_IPC"] = abs(dfWay["IPC:mean"] - dfWay["IPC_prediction:mean"]) / dfWay["IPC:mean"]

                outputPathWays = os.path.abspath(outputPath+"/"+ str(waysCR) + "cr" + str(waysO) + "others")

                os.makedirs(os.path.abspath(outputPathWays), exist_ok=True)

                # create DF for LLC of all apps in workload
                columns = ['interval']
                numRows = dfWay['IPC:mean'].count()/numApps
                numRows = np.asscalar(np.int16(numRows))
                index = range(0, numRows)
                dfLLC = pd.DataFrame(columns=columns, index = index)
                dfLLC["interval"] = range(0, numRows)

                LLCDATAF = dfLLC

                for appName, df in groups:
                    print(appName)

                    #generate interval_data_table
                    outputPathWaysWorkload = outputPathWays + "/" + wl_show_name
                    os.makedirs(os.path.abspath(outputPathWaysWorkload), exist_ok=True)
                    outputPathApp = outputPathWaysWorkload + "/" + appName + "-intervalDataTable.csv"
                    df.to_csv(outputPathApp, sep=',')

                    #add column to LLC_occup dataframe
                    #nameC = "LLC" + app
                    #print(list(dfWayAux.columns.values))
                    AuxAPP = dfWayAux[dfWayAux.app.isin([appName])]
                    AuxAPP = AuxAPP['l3_Mbytes_occ:mean']
                    AuxAPP.index = range(0, numRows)
                    #print(AuxAPP)
                    LLCDATAF[appName] = AuxAPP

                # save LLC_occup dataframe
                #print(LLCDATAF)
                LLCDATAF = LLCDATAF.set_index(['interval'])
                outputPathLLC = outputPathWaysWorkload + "/LLC_occup_apps_data_table.csv"
                LLCDATAF.to_csv(outputPathLLC, sep=',')


                groups = dfWay.groupby(level=[0])
                columns = ['interval','IPC:mean','IPC_prediction:mean','rel_err_IPC','IPC:ci','MPKIL3:mean','MPKIL3:ci']
                numRows = dfWay['IPC:mean'].count()/numApps
                numRows = np.asscalar(np.int16(numRows))
                index = range(0, numRows + 1)
                dfTotWays = pd.DataFrame(columns=columns, index = index)
                dfTotWays['interval'] = range(0, numRows + 1)
                dfTotWays = dfTotWays.set_index(['interval'])
                for interval, df in groups:
                    #dfTotWays.ix[interval, 'interval'] = interval
                    dfTotWays.ix[interval, 'IPC:mean'] = df['IPC:mean'].sum()
                    dfTotWays.ix[interval, 'IPC:ci'] = np.sqrt((df['IPC:ci']**2).sum())
                    dfTotWays.ix[interval, 'MPKIL3:mean'] = df['MPKIL3:mean'].sum()
                    dfTotWays.ix[interval, 'MPKIL3:ci'] = np.sqrt((df['MPKIL3:ci']**2).sum())
                    dfTotWays.ix[interval, 'IPC_prediction:mean'] = df['IPC_prediction:mean'].sum()

                dfTotWays['rel_err_IPC'] = abs(dfTotWays['IPC:mean'] - dfTotWays['IPC_prediction:mean']) / dfTotWays['IPC:mean']
                outputPathTotWays = outputPathWays + "/" + wl_show_name + "-total-table.csv"
                dfTotWays.to_csv(outputPathTotWays, sep=',')


                dfTotal.ix[numConfig,'IPC_prediction:mean'] = dfTotWays['IPC_prediction:mean'].mean()
                dfTotal.ix[numConfig,'median_rel_err_IPC'] = dfTotWays['rel_err_IPC'].median()

                # df with individual values to calculate ANTT and STP
                wl_in_path = args.inputdir + "/" + str(waysCR) + "cr" + str(waysO) + "others/outputFilesMean/" + wl_show_name + "-fin.csv"

                dfway = pd.read_table(wl_in_path, sep=",")
                dfway = dfway.set_index(['app'])
                groups = dfway.groupby(level=[0])

                # dataframe for individual execution
                dfIndiv = pd.DataFrame()

                progress = 0
                slowdown = 0

                progressERR = 0
                slowdownERR = 0

                core = 0

                # calculate the progress and slowdown of each app
                # and add all the values
                for app, df in groups:
                    words = app.split('_')
                    appName = words[1]

                    wl_indiv = "/home/lupones/manager/experiments/npIndivInterval/outputFilesMean/" + appName + "-fin.csv"
                    dfIndiv = pd.read_table(wl_indiv, sep=",")
                    progressApp = df.ix[0,'ipnc:mean'] / dfIndiv.ix[0,'ipnc:mean']
                    slowdownApp = dfIndiv.ix[0,'ipnc:mean'] / df.ix[0,'ipnc:mean']

                    progress = progress + progressApp
                    slowdown = slowdown + slowdownApp

                    relErrComp = (df.ix[0,'ipnc:std'] / df.ix[0,'ipnc:mean'])**2
                    relErrIndiv = (dfIndiv.ix[0,'ipnc:std'] / dfIndiv.ix[0,'ipnc:mean'])**2
                    relERR = np.sqrt(relErrComp + relErrIndiv)

                    progressERR = progressERR + (progressApp*relERR)
                    slowdownERR = slowdownERR + (slowdownApp*relERR)
                    core = core + 1

                # calculate and add values of configuration to total table
                dfTotal.ix[numConfig,'STP:mean'] = progress
                dfTotal.ix[numConfig,'STP:ci'] = Z * (progressERR / 24)

                mean_slowdown = (slowdown / 8)
                unfairness = slowdownERR / mean_slowdown

                unERR = (slowdownERR/8) / mean_slowdown
                unfairnessERR = unfairness*unERR

                dfTotal.ix[numConfig,'Unfairness:mean'] = unfairness
                dfTotal.ix[numConfig,'Unfairness:ci'] = Z * (unfairnessERR / 24)
                dfTotal.ix[numConfig,'ANTT:mean'] = mean_slowdown
                dfTotal.ix[numConfig,'ANTT:ci'] = Z * (unfairness / 24)

                # calculate the Workload Turn Around Time
                path_total = args.inputdir + "/" + str(waysCR) + "cr" + str(waysO) + "others/outputFilesMean/" + wl_show_name + "-tot.csv"
                dft = pd.read_table(path_total, sep=",")

                WTt_value = dft["interval:mean"].iloc[-1]
                WTt_err = dft["interval:std"].iloc[-1]

                dfTotal.ix[numConfig,'Tt:mean'] = WTt_value
                dfTotal.ix[numConfig,'Tt:ci'] = Z * (WTt_err / 3)

                numConfig = numConfig + 1;

            #generate dfTotal table
            outputPathTotal = outputPath + "/" + wl_show_name + "-totalDataTable.csv"
            dfTotal = dfTotal.set_index(['configuration'])
            print(outputPathTotal)
            print(dfTotal)
            dfTotal.to_csv(outputPathTotal, sep=',')


# El main es crida des d'ací
if __name__ == "__main__":
    main()

