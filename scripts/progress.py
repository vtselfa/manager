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


with open(sys.argv[1], 'r') as f:
    workloads = yaml.load(f)

name = sys.argv[2]


result = pd.DataFrame()
for wl_id, wl in enumerate(workloads):
    wl_name = "-".join(wl)
    files = [f for f in os.listdir('.') if re.match(r'{}_[0-9]+_fin.csv$'.format(wl_name), f)]

    assert len(files) > 0, "No files!"

    try:
        wl_df = pd.DataFrame()
        for r, f in enumerate(files):
            df = pd.read_table(f, sep=",")
            print(f)
            df.columns = map(str.strip, df.columns)
            del df["ipc"]
            wl_df = wl_df.append(df)

        groups = wl_df.groupby(["app","core"])
        res = groups.agg([np.mean, ci95, rci95])

        res[("ipc","mean")]  = res[("instructions","mean")]  / res[("cycles","mean")]
        res[("ipc","rci95")] = res[("instructions","rci95")] + res[("cycles","rci95")]
        res[("ipc","ci95")]  = res[("instructions","rci95")]   * res[("ipc","mean")]
        res[("slowdown","mean")]  = res[("us", "mean")] / (60 * 1000 * 1000)
        res[("slowdown","rci95")] = res[("us", "rci95")]

        res["wl_name"] = wl_name
        res["wl_id"] = wl_id
        res.set_index("wl_id", append=True, inplace=True)
        res.set_index("wl_name", append=True, inplace=True)
        res = res.reorder_levels(["wl_name", "wl_id", "app", "core"])
    except:
        print("{}: Unexpected error: {}".format(wl_name, sys.exc_info()[0]))
        print(res.head())
        print(res.columns)
        raise

    result = result.append(res)

groups = result.groupby(level=1)
def funct(x):
    x[("coefvar","mean")] = scipy.stats.variation(x[("progress","mean")])
    x[("stp","mean")] = np.sum(x[("progress","mean")])
    x[("stp","ci95")] = np.sum(x[("progress","ci95")])
    x[("antt","mean")] = np.mean(x[("slowdown","mean")])
    return x

result = groups.apply(funct)


wl_groups = result.groupby(level=[0,1])

to_apply = OrderedDict()
to_apply[("coefvar","mean")] = lambda x: x.values[-1]
to_apply[("stp","mean")] = lambda x: x.values[-1]
to_apply[("antt","mean")] = lambda x: x.values[-1]
to_apply[("instructions","mean")] = sum
to_apply[("ev0","mean")] = sum
to_apply[("ev1","mean")] = sum
to_apply[("ev2","mean")] = sum
to_apply[("ev3","mean")] = sum
to_apply[("l3_kbytes_occ","mean")] = scipy.stats.variation
to_apply[("proc_energy","mean")] = max
to_apply[("dram_energy","mean")] = max
perwl = wl_groups.agg(to_apply)#.reset_index(level=[2,3], drop=True)
perwl.columns = [col + "-" + name for col in ["unfairness", "stp", "antt", "instructions", "ev0sum", "ev1sum", "ev2sum", "ev3sum", "l3_occ_cov", "proc_energy", "dram_energy"]]
perwl["total_energy"] = perwl["proc_energy" + "-" + name] + perwl["dram_energy" + "-" + name]
perwl.sort_index(level="wl_id", inplace=True)


r = result.reset_index(level=[2], drop=True)
r[[("ev3","mean"), ("progress","mean")]].unstack(level=[2]).to_csv("result_ev3.csv")
result.to_csv("result.csv")
perwl.to_csv(name + ".wl.csv")

