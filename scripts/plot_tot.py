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
    parser.add_argument('-c', '--columns', nargs='+', required=True, help='Columns to plot.')
    parser.add_argument('-o', '--output-dir', default='plotint', help='Output dir.')
    parser.add_argument('-t', '--threads', type=int, default=1, help='Number of threads to use.')
    args = parser.parse_args()

    assert len(args.input_dirs) == len(args.names)

    # Read the file with the list of workloads
    with open(args.workloads, 'r') as f:
        workloads = yaml.load(f)

    # Prepare arguments
    arguments = list()
    for wl_id, wl in enumerate(workloads):
        arguments.append((wl, args.columns, args.names, args.input_dirs, args.output_dir))

    # Parallellize
    assert len(arguments), colored("Nothing to read", "red")
    print("." * len(arguments), end="")
    print("\b" * len(arguments), end="")
    if args.threads > 1:
        with Pool(args.threads) as pool:
            pool.starmap(plot_workload, arguments)
    else:
        for data in arguments:
            plot_workload(*data)
    print()



def plot_workload(wl, metrics, names, input_dirs, output_dir):
    wl_name = "-".join(wl)
    num_metrics = len(metrics)

    a = sp.default_args()
    # a.equal_yaxes = [list(range(num_metrics))]
    # a.equal_xaxes = [list(range(num_metrics))]
    a.equal_yaxes = [[]]
    a.equal_xaxes = [[]]
    index = 0

    assert num_metrics <= 12
    if num_metrics == 1:
        grid = [[1,1]]
    elif num_metrics == 2:
        grid = [[2,1]]
    elif num_metrics <= 4:
        grid = [[2,2]]
    elif num_metrics <= 8:
        grid = [[2,2], [2,2]]
    elif num_metrics <= 12:
        grid = [[2,2], [2,2], [2,2]]
    else:
        assert False

    for axnum, metric in enumerate(metrics):
        dataframes = []
        filename = "{}/{}_{}_bars.csv".format(output_dir, wl_name, metric.replace('/','.'))
        for exp_id, (exp_name, input_dir) in enumerate(zip(names, input_dirs)):
            wl_dir = "{}/{}".format(input_dir, wl_name)
            wl_fin = "{}/{}_fin.csv".format(input_dir, wl_name)
            usecols = ["app", metric + ":mean", metric + ":std"]
            df = pd.read_table(wl_fin, sep=",", usecols=usecols)
            df.set_index("app", inplace=True)
            df.columns = [exp_name, exp_name + ":std"]
            dataframes.append(df)
        df = pd.concat(dataframes, axis=1)
        df.to_csv(filename)

        plot = dict()
        plot["kind"] = "b"
        plot["datafile"] = filename
        plot["index"] = 0
        plot["cols"] =  [df.columns.get_loc(n) + 1 for n in names]
        plot["ecols"] = [df.columns.get_loc(n + ":std") + 1 for n in names for _ in range(2)]
        plot["errorbars"] = "both"
        # plot["title"] = metric
        # plot["labels"] = [exp_name]
        # plot["xlabel"] = index_col
        plot["ylabel"] = metric
        # plot["vl"] = [lastint_mean, {"xerr": [0, lastint_std, 0], "color": None}]
        # if exp_id > 0:
        #     plot["axnum"] = axnum
        a.plot.append(plot)


        # args = """ --plot '{kind: b, datafile: data/progress_estimation.csv, index: 0, cols: [1,2], ecols: [3,4], errorbars: max, labels: [ASM,PTCA], ylabel: "Progress Estimation Error", legend_options: {loc: 9, frameon: False, ncol: 2}, xlabel: Number of applications, xrot: 0, ymax: 0.25, ypercent: True}' --size 4 2.5 --dpi 100"""
        # args = simplot.parse_args(shlex.split(args))
        # figs, axes = simplot.create_figures(args.grid, args.size, args.dpi)
        # simplot.plot_data(figs, axes, args.plot, args.title, args.equal_xaxes, args.equal_yaxes, args.rect)
        # fig = figs[0]

    figs, axes = sp.create_figures(grid, a.size, a.dpi)
    sp.plot_data(figs, axes, a.plot, a.title, a.equal_xaxes, a.equal_yaxes, a.rect)
    metric = re.sub('/$', '', metric).replace("/", ".")
    sp.write_output(figs, "{}/{}/bars.pdf".format(output_dir, wl_name), a.rect)
    print("x", end="", flush=True)


if __name__ == "__main__":
    main()
