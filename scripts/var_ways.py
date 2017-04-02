import pandas as pd
import numpy as np
import sys
import scipy.stats
import yaml
import os
import re

from collections import OrderedDict


pd.set_option('display.expand_frame_repr', False)


def mean_confidence_interval(data, confidence=0.95):
    data = data[np.isfinite(data)]
    n = len(data)
    m, se = np.mean(data), scipy.stats.sem(data)
    h = se * scipy.stats.t._ppf((1+confidence)/2., n-1)
    return h

def rel_mean_confidence_interval(data, confidence=0.95):
    data = data[np.isfinite(data)]
    n = len(data)
    m, se = np.mean(data), scipy.stats.sem(data)
    h = se * scipy.stats.t._ppf((1+confidence)/2., n-1)
    return h/m

def ci95(data):
    return mean_confidence_interval(data)

def rci95(data):
    return rel_mean_confidence_interval(data)


def build_file_index(workloads_yaml_file):
    with open(workloads_yaml_file, 'r') as f:
        workloads = yaml.load(f)

    files = dict()
    for wl in workloads:
        wl_name = "-".join(wl)

        for f in os.listdir('.'):
            m = re.match(r'^{}_([0-9a-fA-F]+)_([0-9]+)_fin.csv$'.format(wl_name), f)
            if not m:
                continue
            ways = bin(int(m.group(1), base=16)).count("1")
            rep = int(m.group(2))
            files[(wl_name, ways, rep)] = f

    files = pd.DataFrame.from_dict(files, orient="index")
    files.index = pd.MultiIndex.from_tuples(files.index, names=["wl", "ways", "rep"])
    files.sort_index(inplace=True)
    files.columns = ["file"]
    files.to_csv("files.csv")

    return files


def read_files(file_index):
    data = pd.DataFrame()
    for group in file_index.groupby(level=[0, 1]):
        for row in group[1].itertuples():
            wl, ways, rep = row[0]
            f = row[1]
            df = pd.read_table(f, sep=",")
            df["app"] = wl
            df["ways"] = ways
            df["rep"] = rep
            assert len(df) == 1, "File '{}' should have 1 row".format(f)
            data = data.append(df)
    data = data.reset_index(drop=True)
    return data

data = read_files(build_file_index(sys.argv[1]))

for group in data.groupby(["app", "ways"]):
    wl, ways = group[0]
    df = group[1]
    f = "{}_{}_all_fin.csv".format(wl, ways)
    df.to_csv(f, float_format="%.5f", index=False)


try:
    del data["ipc"]
    del data["ipnc"]
    del data["rep"]
    del data["core"]

    groups = data.groupby(["app", "ways"])
    res = groups.agg([np.mean, ci95, rci95])

    res[("ipc","mean")]  = res[("instructions","mean")]  / res[("cycles","mean")]
    res[("ipc","rci95")] = res[("instructions","rci95")] + res[("cycles","rci95")]
    res[("ipc","ci95")]  = res[("instructions","rci95")] * res[("ipc","mean")]

    res[("ipnc","mean")]  = res[("instructions","mean")]  / res[("invariant_cycles","mean")]
    res[("ipnc","rci95")] = res[("instructions","rci95")] + res[("invariant_cycles","rci95")]
    res[("ipnc","ci95")]  = res[("instructions","rci95")] * res[("ipnc","mean")]
except:
    print("{}: Unexpected error: {}".format(wl_name, sys.exc_info()[0]))
    print(res.head())
    print(res.columns)
    raise

print(res.head())
res[("progress","mean")]  = 60 * 1000 * 1000 / res[("us", "mean")]
res[("progress","rci95")] = res[("us", "rci95")]
res[("progress","ci95")]  = res[("progress","mean")] * res[("progress","rci95")]
res.to_csv("var_ways.csv", float_format="%.5f")
sys.exit(1)

result = result.append(res)

groups = result.groupby(level=1)
def funct(x):
    x[("coefvar","mean")] = scipy.stats.variation(x[("progress","mean")])
    x[("throghput","mean")] = np.sum(x[("progress","mean")])
    x[("throghput","ci95")] = np.sum(x[("progress","ci95")])
    x[("throghput","rci95")] = np.sum(x[("progress","rci95")])
    return x

result = groups.apply(funct)







result = pd.DataFrame()
for wl_id, wl in enumerate(workloads):
    wl_name = "-".join(wl)
    files = [f for f in os.listdir('.') if re.match(r'{}_[0-9a-fA-F]+_[0-9]+_fin.csv$'.format(wl_name), f)]

    assert len(files) > 0, "No files!"

    group_keys = ["app","core","mask"]
    try:
        wl_df = pd.DataFrame()
        for r, f in enumerate(files):
            print(f)
            df = pd.read_table(f, sep=",")
            m = re.match(r'{}_([0-9a-fA-F]+)_[0-9]+_fin.csv$'.format(wl_name), f)

            if m.group(0):
                masks = True
                df["mask"] = int(m.group(1)[1:], base=16)

            df.columns = map(str.strip, df.columns)
            del df["ipc"]
            wl_df = wl_df.append(df)

        if masks:
            group_keys.append("mask")

        groups = wl_df.groupby(group_keys)
        res = groups.agg([np.mean, ci95, rci95])

        res[("ipc","mean")]  = res[("instructions","mean")]  / res[("cycles","mean")]
        res[("ipc","rci95")] = res[("instructions","rci95")] + res[("cycles","rci95")]
        res[("ipc","ci95")]  = res[("instructions","rci95")]   * res[("ipc","mean")]

        res[("progress","mean")]  = 60 * 1000 * 1000 / res[("us", "mean")]
        res[("progress","rci95")] = res[("us", "rci95")]
        res[("progress","ci95")]  = res[("progress","mean")] * res[("progress","rci95")]

        res["wl_name"] = wl_name
        res["wl_id"] = wl_id
        res.set_index("wl_id", append=True, inplace=True)
        res.set_index("wl_name", append=True, inplace=True)
        res = res.reorder_levels(["wl_name", "wl_id"] + group_keys)
    except:
        print("{}: Unexpected error: {}".format(wl_name, sys.exc_info()[0]))
        print(res.head())
        print(res.columns)
        raise

    result = result.append(res)

groups = result.groupby(level=1)
def funct(x):
    x[("coefvar","mean")] = scipy.stats.variation(x[("progress","mean")])
    x[("throghput","mean")] = np.sum(x[("progress","mean")])
    x[("throghput","ci95")] = np.sum(x[("progress","ci95")])
    x[("throghput","rci95")] = np.sum(x[("progress","rci95")])
    return x

result = groups.apply(funct)


wl_groups = result.groupby(level=[0,1])

to_apply = OrderedDict()
to_apply[("coefvar","mean")] = lambda x: x.values[-1]
to_apply[("throghput","mean")] = lambda x: x.values[-1]
to_apply[("ev0","mean")] = sum
to_apply[("ev1","mean")] = sum
to_apply[("ev2","mean")] = sum
to_apply[("ev3","mean")] = sum
to_apply[("l3_kbytes_occ","mean")] = scipy.stats.variation
to_apply[("proc_energy","mean")] = max
to_apply[("dram_energy","mean")] = max
perwl = wl_groups.agg(to_apply)#.reset_index(level=[2,3], drop=True)
perwl.columns = ["unfairness", "throghput", "ev0sum", "ev1sum", "ev2sum", "ev3sum", "l3_occ_cov", "proc_energy", "dram_energy"]
perwl.sort_index(level="wl_id", inplace=True)


r = result.reset_index(level=[2], drop=True)
r[[("ev3","mean"), ("progress","mean")]].unstack(level=[2]).to_csv("result_ev3.csv")
result.to_csv("result.csv")
perwl.to_csv("result_wl.csv")
