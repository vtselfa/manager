#include <linux/time64.h>

#include "util/drv_configs.h"
#include "util/stat.h"
#include "util/thread_map.h"

#include "libminiperf.h"


static inline void diff_timespec(struct timespec *r, struct timespec *a,
				 struct timespec *b)
{
	r->tv_sec = a->tv_sec - b->tv_sec;
	if (a->tv_nsec < b->tv_nsec)
	{
		r->tv_nsec = a->tv_nsec + NSEC_PER_SEC - b->tv_nsec;
		r->tv_sec--;
	}
	else
	{
		r->tv_nsec = a->tv_nsec - b->tv_nsec ;
	}
}


static int create_perf_stat_counter(struct perf_evlist *evsel_list, struct perf_evsel *evsel, struct target *target)
{
	struct perf_event_attr *attr = &evsel->attr;

	attr->read_format = PERF_FORMAT_TOTAL_TIME_ENABLED |
				    PERF_FORMAT_TOTAL_TIME_RUNNING;

	attr->inherit = true;

	/*
	 * Some events get initialized with sample_(period/type) set,
	 * like tracepoints. Clear it up for counting.
	 */
	attr->sample_period = 0;

	/*
	 * But set sample_type to PERF_SAMPLE_IDENTIFIER, which should be harmless
	 * while avoiding that older tools show confusing messages.
	 *
	 * However for pipe sessions we need to keep it zero,
	 * because script's perf_evsel__check_attr is triggered
	 * by attr->sample_type != 0, and we can't run it on
	 * stat sessions.
	 */
	attr->sample_type = PERF_SAMPLE_IDENTIFIER;

	/*
	 * Disabling all counters initially, they will be enabled
	 * either manually by us or by kernel via enable_on_exec
	 * set later.
	 */
	if (perf_evsel__is_group_leader(evsel))
	{
		attr->disabled = 1;

		/*
		 * In case of initial_delay we enable tracee
		 * events manually.
		 */
		if (target__none(target))
			attr->enable_on_exec = 1;
	}

	if (target__has_cpu(target))
		return perf_evsel__open_per_cpu(evsel, perf_evsel__cpus(evsel));

	return perf_evsel__open_per_thread(evsel, evsel_list->threads);
}


/*
 * Read out the results of a single counter
 */
static int read_counter(struct perf_evlist *evsel_list, struct perf_evsel *counter)
{
	int nthreads = thread_map__nr(evsel_list->threads);
	int ncpus = perf_evsel__nr_cpus(counter);

	if (!counter->supported)
		return -ENOENT;

	for (int thread = 0; thread < nthreads; thread++)
	{
		for (int cpu = 0; cpu < ncpus; cpu++)
		{
			struct perf_counts_values *count = perf_counts(counter->counts, cpu, thread);
			if (perf_evsel__read(counter, cpu, thread, count))
				return -1;
		}
	}

	return 0;
}


void read_counters(struct perf_evlist *evsel_list, const char **names, double *results, const char **units, bool *snapshot, uint64_t *enabled, uint64_t *running)
{
	struct perf_evsel *counter;
	struct perf_stat_config stat_config =
	{
		.aggr_mode	= AGGR_GLOBAL,
		.scale		= true,
	};

	evlist__for_each_entry(evsel_list, counter)
	{
		if (read_counter(evsel_list, counter))
			fprintf(stderr, "failed to read counter %s\n", counter->name);

		if (perf_stat_process_counter(&stat_config, counter))
			fprintf(stderr, "failed to process counter %s\n", counter->name);
	}

	size_t i = 0;
	evlist__for_each_entry(evsel_list, counter)
	{
		int nthreads = thread_map__nr(counter->threads);
		int ncpus = cpu_map__nr(counter->cpus);
		uint64_t ena = 0, run = 0, val = 0;

		for (int thread = 0; thread < nthreads; thread++)
		{
			for (int cpu = 0; cpu < ncpus; cpu++)
			{
				val += perf_counts(counter->counts, cpu, thread)->val;
				ena += perf_counts(counter->counts, cpu, thread)->ena;
				run += perf_counts(counter->counts, cpu, thread)->run;
			}
			assert(run <= ena);
		}
		if (names)
			names[i] = counter->name;
		if (results)
			results[i] = val * counter->scale;
		if (units)
			units[i] = counter->unit;
		if (snapshot)
			snapshot[i] = counter->snapshot;
		if (enabled)
			enabled[i] = ena;
		if (running)
			running[i] = run;
		i++;
	}
}


void get_names(struct perf_evlist *evsel_list, const char **names)
{
	struct perf_evsel *counter;

	size_t i = 0;
	evlist__for_each_entry(evsel_list, counter)
	{
		if (names)
			names[i] = counter->name;
		i++;
	}
}


void enable_counters(struct perf_evlist *evsel_list)
{
	/*
	 * We need to enable counters only if:
	 * - we don't have tracee (attaching to task or cpu)
	 * - we have initial delay configured
	 */
	perf_evlist__enable(evsel_list);
}


void disable_counters(struct perf_evlist *evsel_list)
{
	/*
	 * If we don't have tracee (attaching to task or cpu), counters may
	 * still be running. To get accurate group ratios, we must stop groups
	 * from counting before reading their constituent counters.
	 */
	perf_evlist__disable(evsel_list);
}


void print_counters2(struct perf_evlist *evsel_list, struct timespec *ts)
{
	struct perf_evsel *counter;

	evlist__for_each_entry(evsel_list, counter)
	{
		struct perf_stat_evsel *ps = counter->priv;
		double avg = avg_stats(&ps->res_stats[0]);
		double uval;
		double ena, run;

		ena = avg_stats(&ps->res_stats[1]);
		run = avg_stats(&ps->res_stats[2]);

		uval = avg * counter->scale;
		fprintf(stdout, "%f %s %s", uval, counter->unit, counter->name);
		if (run != ena)
			fprintf(stdout, "  (%.2f%%)", 100.0 * run / ena);
		fprintf(stdout, "\n");
	}

	fflush(stdout);
}


struct perf_evlist* setup_events(const char *pid, const char *events)
{
	struct perf_evlist	*evsel_list = NULL;
	bool group = false;

	struct target target = {
		.uid	= UINT_MAX,
		.pid = pid,
	};
	target__validate(&target);

	evsel_list = perf_evlist__new();
	if (evsel_list == NULL)
		return NULL;

	if (parse_events(evsel_list, events, NULL))
	{
		goto out;
	}

	if (perf_evlist__create_maps(evsel_list, &target) < 0)
	{
		assert(target__has_task(&target));
		pr_err("Problems finding threads of monitor\n");
		goto out;
	}
	cpu_map__put(evsel_list->cpus);
	thread_map__put(evsel_list->threads);

	if (perf_evlist__alloc_stats(evsel_list, true))
		goto out;

	if (group)
		perf_evlist__set_leader(evsel_list);


	struct perf_evsel *counter;
	evlist__for_each_entry(evsel_list, counter)
	{
		if (create_perf_stat_counter(evsel_list, counter, &target) < 0)
			exit(-1);
		counter->supported = true;
	}

	if (perf_evlist__apply_filters(evsel_list, &counter))
	{
		error("failed to set filter \"%s\" on event %s with %d (%s)\n",
			counter->filter, perf_evsel__name(counter), errno, strerror(errno));
		exit(-1);
	}

	struct perf_evsel_config_term *err_term;
	if (perf_evlist__apply_drv_configs(evsel_list, &counter, &err_term))
	{
		error("failed to set config \"%s\" on event %s with %d (%s)\n",
		      err_term->val.drv_cfg, perf_evsel__name(counter), errno, strerror(errno));
		exit(-1);
	}

	return evsel_list;
out:
	perf_evlist__delete(evsel_list);
	return NULL;
}


void print_counters(struct perf_evlist *evsel_list)
{
	struct perf_evsel *counter;
	evlist__for_each_entry(evsel_list, counter)
	{
		int nthreads = thread_map__nr(counter->threads);
		int ncpus = cpu_map__nr(counter->cpus);
		uint64_t ena = 0, run = 0, val = 0;
		double uval;

		for (int thread = 0; thread < nthreads; thread++)
		{
			for (int cpu = 0; cpu < ncpus; cpu++)
			{
				val += perf_counts(counter->counts, cpu, thread)->val;
				ena += perf_counts(counter->counts, cpu, thread)->ena;
				run += perf_counts(counter->counts, cpu, thread)->run;
			}
		}
		uval = val * counter->scale;
		fprintf(stdout, "%f %s %s", uval, counter->unit, counter->name);
		if (run != ena)
			fprintf(stdout, "  (%.2f%%)", 100.0 * run / ena);
		fprintf(stdout, "\n");
	}
}


void clean(struct perf_evlist *evlist)
{
	/*
	 * Closing a group leader splits the group, and as we only disable
	 * group leaders, results in remaining events becoming enabled. To
	 * avoid arbitrary skew, we must read all counters before closing any
	 * group leaders.
	 */
	disable_counters(evlist);
	read_counters(evlist, NULL, NULL, NULL, NULL, NULL, NULL);
	perf_evlist__close(evlist);
	perf_evlist__free_stats(evlist);
	perf_evlist__delete(evlist);
}


int num_entries(struct perf_evlist *evsel_list)
{
	return evsel_list->nr_entries;
}
