import argparse
import numpy as np
import os
import os.path as osp
import pandas as pd
import re
import yaml


def main():
    parser = argparse.ArgumentParser(description = 'Aggregates interval stats for different runs of the same workload.')
    parser.add_argument('-w', '--workloads', required=True, help='YAML file with the workloads to process.')
    parser.add_argument('-i', '--input-dir', required=True, help='Input data dir.')
    parser.add_argument('-o', '--output-dir', default='dataint', help='Output dir.')
    parser.add_argument('-ft', '--fileType', action='append', default=[], help='Type of files we want to show results of. Options: tot,fin.')  
    args = parser.parse_args()
  
    for func in args.fileType:
        assert( (func == "fin") | (func == "tot") )


    # Read the file with the list of workloads
    with open(args.workloads, 'r') as f:
        workloads = yaml.load(f)

    # For each workload...
    for wl_id, wl in enumerate(workloads):

        # There is one file for each run of the workload
        wl_name = "-".join(wl)
        filestot = ["{}/{}".format(args.input_dir, f) for f in os.listdir(args.input_dir) if re.match(r'{}_[0-9]+_tot.csv$'.format(wl_name), f)]
        filesfin = ["{}/{}".format(args.input_dir, f) for f in os.listdir(args.input_dir) if re.match(r'{}_[0-9]+_fin.csv$'.format(wl_name), f)] 
        for typeFile in args.fileType:
            fname = "files" + typeFile
            print(fname)
            # Read and merge the different files
            dfs = list()
            for f in eval(fname):
                print(f)
                dfs.append(pd.read_table(f, sep=","))
            dfs = pd.concat(dfs)
            dfs.set_index(["app", "core"], inplace=True)
            groups = dfs.groupby(level=[0, 1])
            dfs = groups.agg([np.mean, np.std])


            # Squash the column multiindex from 2 levels to 1
            dfs.columns = [dfs.columns.map('{0[0]}:{0[1]}'.format)]
            print(dfs)


            # Store csv
            outputname = args.output_dir + "/" + wl_name + "-" + typeFile + ".csv"
            #dfs.to_csv("{}/{}-"+typeFile+".csv".format(args.output_dir, wl_name))
            print(outputname)
            dfs.to_csv(outputname)

if __name__ == "__main__":
    main()
