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
    args = parser.parse_args()


    # Read the file with the list of workloads
    with open(args.workloads, 'r') as f:
        workloads = yaml.load(f)

    # For each workload...
    for wl_id, wl in enumerate(workloads):

        # There is one file for each run of the workload
        wl_name = "-".join(wl)
        files = ["{}/{}".format(args.input_dir, f) for f in os.listdir(args.input_dir) if re.match(r'{}_[0-9]+.csv$'.format(wl_name), f)]

        # Read and merge the different files
        dfs = list()
        for f in files:
            dfs.append(pd.read_table(f, sep=","))
        dfs = pd.concat(dfs)
        dfs.set_index(["interval", "app", "core"], inplace=True)
        groups = dfs.groupby(level=[0, 1, 2])
        dfs = groups.agg([np.mean, np.std])

        # Squash the column multiindex from 2 levels to 1
        dfs.columns = [dfs.columns.map('{0[0]}:{0[1]}'.format)]

        # Create output dir
        os.makedirs(osp.abspath(args.output_dir), exist_ok=True)

        # Store csv
        dfs.to_csv("{}/{}.csv".format(args.output_dir, wl_name))

        # Store a csv per app
        os.makedirs(osp.abspath("{}/{}".format(args.output_dir, wl_name)), exist_ok=True)
        for (app, core), df in dfs.groupby(level=[1, 2]):
            filename = "{}/{}/{}.{}.csv".format(args.output_dir, wl_name, app, core)
            df.to_csv(filename)


if __name__ == "__main__":
    main()
