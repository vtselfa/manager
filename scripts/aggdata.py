import argparse
import numpy as np
import os
import os.path as osp
import pandas as pd
import re
import yaml


def main():
    parser = argparse.ArgumentParser(description = 'Aggregates stats for different runs of the same workload.')
    parser.add_argument('-w', '--workloads', required=True, help='YAML file with the workloads to process.')
    parser.add_argument('-i', '--input-dir', required=True, help='Input data dir.')
    parser.add_argument('-n', '--name', required=True, help='Exec name.')
    parser.add_argument('--alone', required=True, type=int, help='Number of intervals that takes to execute the applications alone. Quotient for the slowdown.')
    parser.add_argument('-o', '--output-dir', default='aggrdata', help='Output dir.')
    args = parser.parse_args()

    # Create output dir
    os.makedirs(osp.abspath(args.output_dir), exist_ok=True)

    # Store name
    with open(args.output_dir + "/name", 'w') as f:
        f.write(args.name + "\n")

    # Read the file with the list of workloads
    with open(args.workloads, 'r') as f:
        workloads = yaml.load(f)

    # For each workload...
    for wl_id, wl in enumerate(workloads):
        if isinstance(wl, str):
            wl = [wl]
        try:
            process_intdata(wl, args.input_dir, args.output_dir, args.alone)
            process_data(wl, args.input_dir, args.output_dir, args.alone)
        except Exception as e:
            print("Error in {}: {}".format(wl, e))


def process_intdata(workload, input_dir, output_dir, alone):
    # There is one file for each run of the workload
    wl_name = "-".join(workload)
    files = ["{}/{}".format(input_dir, f) for f in os.listdir(input_dir) if re.match(r'{}_[0-9]+.csv$'.format(wl_name), f)]

    if len(files) == 0:
        print("The workload {} has no 'int' files".format(wl_name))
        return

    dfs = read_and_merge(files, ["interval", "app"], alone)

    # Store csv
    dfs.to_csv("{}/{}.csv".format(output_dir, wl_name))

    # Store a csv per app
    os.makedirs(osp.abspath("{}/{}".format(output_dir, wl_name)), exist_ok=True)
    for app, df in dfs.groupby(level=1):
        filename = "{}/{}/{}.csv".format(output_dir, wl_name, app)
        df.to_csv(filename)


def process_data(workload, input_dir, output_dir, alone):
    for kind in ["fin", "tot"]:
        # There is one file for each run of the workload
        wl_name = "-".join(workload)
        files = ["{}/{}".format(input_dir, f) for f in os.listdir(input_dir) if re.match(r'{}_[0-9]+_{}.csv$'.format(wl_name, kind), f)]

        if len(files) == 0:
            print("The workload {} has no '{}' files".format(wl_name, kind))
            continue

        dfs = read_and_merge(files, ["app"], alone)

        # Store csv
        dfs.to_csv("{}/{}_{}.csv".format(output_dir, wl_name, kind))


def read_and_merge(files, index, alone):
    dfs = list()
    for f in files:
        try:
            df = pd.read_table(f, sep=",")
        except:
            print("Warning: could not read '{}'".format(f))
            continue

        df["progress"] = alone / df["interval"]
        df["slowdown"] = df["interval"] / alone
        df["stp"] = sum(df["progress"])
        df["antt"] = np.mean(df["slowdown"])
        df["unfairness"] = df["progress"].std() / df["progress"].mean()
        dfs.append(df)

    if len(dfs) == 0:
        raise Exception("No files could be read for the workload")

    dfs = pd.concat(dfs)
    dfs.set_index(index, inplace=True)
    groups = dfs.groupby(level=list(range(len(index))))
    dfs = groups.agg([np.mean, np.std])

    # Squash the column multiindex from 2 levels to 1
    dfs.columns = [dfs.columns.map('{0[0]}:{0[1]}'.format)]

    return dfs


if __name__ == "__main__":
    main()
