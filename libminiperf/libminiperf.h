#pragma once

struct perf_evlist;

void read_counters(struct perf_evlist *evsel_list);
void enable_counters(struct perf_evlist *evsel_list);
void disable_counters(struct perf_evlist *evsel_list);
struct perf_evlist* setup_events(const char *pid, const char *events);
void print_counters(struct perf_evlist *evsel_list);
void clean(struct perf_evlist *evlist);
