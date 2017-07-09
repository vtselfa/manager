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
    parser = argparse.ArgumentParser(description = 'Aggregates interval stats for different runs of the same workload.')
    parser.add_argument('-w', '--workloads', required=True, help='YAML file with the workloads to process.')
    parser.add_argument('-i', '--input-dirs', nargs='+', required=True, help='Input data dirs.')
    parser.add_argument('-n', '--names', nargs='+', required=True, help='Names for the input data dirs.')
    parser.add_argument('-o', '--output-dir', default='plotint', help='Output dir.')
    parser.add_argument('-t', '--threads', type=int, default=1, help='Number of threads to use.')
    args = parser.parse_args()

    assert len(args.input_dirs) == len(args.names)

    # Read the file with the list of workloads
    with open(args.workloads, 'r') as f:
        workloads = yaml.load(f)

    # Prepare arguments
    arguments = list()
    for metric in ["ipnc", "l3_kbytes_occ", "mc_gbytes_rd", "mc_gbytes_wt", "proc_energy", "dram_energy", "ev0", "ev1", "ev2", "ev3"]:
        for wl_id, wl in enumerate(workloads):
            arguments.append((wl, metric, args.names, args.input_dirs, args.output_dir))

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



def plot_workload(wl, metric, names, input_dirs, output_dir):
    wl_name = "-".join(wl)
    num_apps = len(wl)

    a = sp.default_args()
    a.equal_yaxes = [list(range(num_apps))]
    index = 0

    # For each experiment
    for exp_id, (exp_name, input_dir) in enumerate(zip(names, input_dirs)):
        wl_dir = "{}/{}".format(input_dir, wl_name)
        files = ["{}/{}".format(wl_dir, f) for f in os.listdir(wl_dir) if re.match(r'.+[.]csv$', f)]
        files.sort()

        assert num_apps == len(files)
        assert num_apps == 1 or num_apps % 2 == 0
        assert num_apps <= 8
        if num_apps == 1:
            grid = [[1,1]]
        elif num_apps == 2:
            grid = [[2,1]]
        elif num_apps == 4:
            grid = [[2,2]]
        elif num_apps == 8:
            grid = [[2,2], [2,2]]
        else:
            assert False

        for axnum, f in enumerate(files):
            r1 = pd.read_table(f, sep=",", nrows=1)
            index_col = r1.columns[index]
            compl = pd.read_table(f, sep=",", index_col=index, usecols=[index_col, "compl:mean", "compl:std"])
            compl = compl[compl["compl:mean"] < 1].tail(1)
            compl_mean = compl["compl:mean"].values[0] * compl.index.values[0]
            compl_std = compl["compl:std"].values[0] * compl.index.values[0]

            plot = dict()
            plot["kind"] = "line"
            plot["datafile"] = f
            plot["index"] = index
            plot["cols"] =  [r1.columns.get_loc(metric + ":mean")]
            plot["ecols"] = [r1.columns.get_loc(metric + ":std")]
            plot["title"] = r1["app"].values[0]
            plot["labels"] = [exp_name]
            plot["xlabel"] = index_col
            plot["ylabel"] = metric
            plot["vl"] = [compl_mean, {"xerr": [0, compl_std, 0], "color": None}]
            if exp_id > 0:
                plot["axnum"] = axnum
            a.plot.append(plot)
    figs, axes = sp.create_figures(grid, a.size, a.dpi)
    sp.plot_data(figs, axes, a.plot, a.title, a.equal_xaxes, a.equal_yaxes, a.rect)
    sp.write_output(figs, "{}/{}/{}.pdf".format(output_dir, wl_name, metric), a.rect)
    print("x", end="", flush=True)


if __name__ == "__main__":
    main()
