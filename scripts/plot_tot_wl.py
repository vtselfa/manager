import argparse
import sys
import pandas as pd
import re
import yaml
import os

from multiprocessing import Pool

sys.path.append("/home/viselol/simplot")
import simplot as sp


def main():
    parser = argparse.ArgumentParser(description = 'Aggregates total stats for different runs of the same workload.')
    parser.add_argument('-w', '--workloads', required=True, help='YAML file with the workloads to process.')
    parser.add_argument('-i', '--input-dirs', nargs='+', required=True, help='Input data dirs.')
    parser.add_argument('-n', '--names', nargs='+', required=True, help='Names for the input data dirs.')
    parser.add_argument('-o', '--output-dir', default='plotint', help='Output dir.')
    parser.add_argument('-t', '--threads', type=int, default=1, help='Number of threads to use.')
    parser.add_argument('-c', '--columns', type=yaml.load, default=dict(), help='Dictionary with the coluluns to plot and the opperation to perform on them.')
    args = parser.parse_args()

    assert len(args.input_dirs) == len(args.names)

    # Read the file with the list of workloads
    with open(args.workloads, 'r') as f:
        workloads = yaml.load(f)

    # Prepare arguments
    arguments = list()
    for metric, agg in args.columns.items():
        arguments.append((workloads, metric, agg, args.names, args.input_dirs, args.output_dir))

    # Parallellize
    assert len(arguments), colored("Nothing to read", "red")
    print("." * len(arguments), end="")
    print("\b" * len(arguments), end="")
    if args.threads > 1:
        with Pool(args.threads) as pool:
            pool.starmap(plot_metric, arguments)
    else:
        for data in arguments:
            plot_metric(*data)
    print()



def plot_metric(workloads, metric, agg, names, input_dirs, output_dir):
    a = sp.default_args()
    # a.equal_yaxes = [list(range(num_metrics))]
    # a.equal_xaxes = [list(range(num_metrics))]
    a.equal_yaxes = [[]]
    a.equal_xaxes = [[]]
    index = 0

    filename = "{}/{}_wls.csv".format(output_dir, metric.replace('/','.'))
    dfs_metric = []
    for wl_id, wl in enumerate(workloads):
        wl_name = "-".join(wl)
        dfs_exp = []
        for exp_id, (exp_name, input_dir) in enumerate(zip(names, input_dirs)):
            wl_dir = "{}/{}".format(input_dir, wl_name)
            wl_fin = "{}/{}_fin.csv".format(input_dir, wl_name)
            usecols = ["app", metric + ":mean", metric + ":std"]
            df = pd.read_table(wl_fin, sep=",", usecols=usecols)
            df.set_index("app", inplace=True)
            df.columns = ["{}:{}".format(exp_name, metric), "{}:{}:std".format(exp_name, metric)]
            dfs_exp.append(df)
        df = pd.concat(dfs_exp, axis=1) # For this workload we have the data from all the experiments

        if agg == "sum":
            df = pd.DataFrame(df.sum()).T
        elif agg == "max":
            df = pd.DataFrame(df.max()).T
        else
            assert(1==2)

        dfs_metric.append(df)
    df = pd.concat(dfs_metric, axis=0).reset_index(drop=True) # We have data for all the workloads
    print(df)
    df.to_csv(filename)

    plot = dict()
    plot["kind"] = "l"
    plot["datafile"] = filename
    plot["index"] = 0
    plot["cols"] =  [df.columns.get_loc("{}:{}".format(n, metric)) + 1 for n in names]
    plot["ecols"] = [df.columns.get_loc("{}:{}:std".format(n, metric)) + 1 for n in names]
    # plot["errorbars"] = "both"
    # plot["title"] = metric
    plot["labels"] = names
    plot["xlabel"] = "Workloads"
    plot["ylabel"] = metric
    # plot["vl"] = [lastint_mean, {"xerr": [0, lastint_std, 0], "color": None}]
    # if exp_id > 0:
    #     plot["axnum"] = axnum
    a.plot.append(plot)

    grid = [[1,1]]
    figs, axes = sp.create_figures(grid, a.size, a.dpi)
    sp.plot_data(figs, axes, a.plot, a.title, a.equal_xaxes, a.equal_yaxes, a.rect)
    metric = re.sub('/$', '', metric).replace("/", ".")
    sp.write_output(figs, "{}/{}_{}.pdf".format(output_dir, metric, agg), a.rect)
    print("x", end="", flush=True)


if __name__ == "__main__":
    main()
