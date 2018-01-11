#pragma once

#include <stdbool.h>
#include <stdint.h>

struct perf_evlist;

void read_counters(struct perf_evlist *evsel_list, const char **names, double *results, const char **units, bool *snapshot, uint64_t *enabled, uint64_t *running);
void get_names(struct perf_evlist *evsel_list, const char **names);
void enable_counters(struct perf_evlist *evsel_list);
void disable_counters(struct perf_evlist *evsel_list);
struct perf_evlist* setup_events(const char *pid, const char *events);
void print_counters(struct perf_evlist *evsel_list);
void clean(struct perf_evlist *evlist);
int num_entries(struct perf_evlist *evsel_list);
