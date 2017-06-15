#include "events-perf.hpp"
extern "C"
{
#include "libminiperf.h"
}


void Perf::init()
{}


void Perf::clean()
{
	for (const auto &item : pid_events)
		for (const auto &evlist : item.second.groups)
			::clean(evlist);
}


void Perf::setup_events(pid_t pid, const std::vector<std::string> &groups)
{
	for (const auto &events : groups)
	{
		const auto evlist = ::setup_events(std::to_string(pid).c_str(), events.c_str());
		pid_events[pid].append(evlist);
		::enable_counters(evlist);
	}
}


void Perf::read_groups(pid_t pid)
{
	for (const auto &evlist : pid_events[pid].groups)
		::read_counters(evlist);
}


void Perf::print_counts(pid_t pid)
{
	for (const auto &evlist : pid_events[pid].groups)
		::print_counters(evlist);
}
