#include <fmt/format.h>

extern "C"
{
#include <libminiperf.h>
}

#include "events-perf.hpp"
#include "throw-with-trace.hpp"


using fmt::literals::operator""_format;


void Perf::init()
{}


void Perf::clean()
{
	for (const auto &item : pid_events)
		for (const auto &evlist : item.second.groups)
			::clean(evlist);
}


void Perf::clean(pid_t pid)
{
	for (const auto &evlist : pid_events.at(pid).groups)
		::clean(evlist);
	pid_events.erase(pid);
}


void Perf::setup_events(pid_t pid, const std::vector<std::string> &groups)
{
	assert(pid >= 1);
	for (const auto &events : groups)
	{
		const auto evlist = ::setup_events(std::to_string(pid).c_str(), events.c_str());
		if (evlist == NULL)
			throw_with_trace(std::runtime_error("Could not setup events '{}'"_format(events)));
		if (::num_entries(evlist) >= max_num_events)
			throw_with_trace(std::runtime_error("Too many events"));
		pid_events[pid].append(evlist);
		::enable_counters(evlist);
	}
}


void Perf::enable_counters(pid_t pid)
{
	for (const auto &evlist : pid_events[pid].groups)
		::enable_counters(evlist);
}


void Perf::disable_counters(pid_t pid)
{
	for (const auto &evlist : pid_events[pid].groups)
		::disable_counters(evlist);
}


std::vector<counters_t> Perf::read_counters(pid_t pid)
{
	const char *names[max_num_events];
	double results[max_num_events];
	const char *units[max_num_events];
	bool snapshot[max_num_events];
	double enabled[max_num_events];

	auto result = std::vector<counters_t>();

	for (const auto &evlist : pid_events[pid].groups)
	{
		int n = ::num_entries(evlist);
		auto counters = counters_t();
		::read_counters(evlist, names, results, units, snapshot, enabled);
		for (int i = 0; i < n; i++)
			counters.insert({i, names[i], results[i], units[i], snapshot[i], enabled[i]});
		result.push_back(counters);
	}
	return result;
}


std::vector<std::vector<std::string>> Perf::get_names(pid_t pid)
{
	const char *names[max_num_events];
	auto r = std::vector<std::vector<std::string>>();

	for (const auto &evlist : pid_events[pid].groups)
	{
		int n = ::num_entries(evlist);
		auto v = std::vector<std::string>();
		::get_names(evlist, names);
		for (int i = 0; i < n; i++)
			v.push_back(names[i]);
		r.push_back(v);
	}
	return r;
}


void Perf::print_counters(pid_t pid)
{
	for (const auto &evlist : pid_events[pid].groups)
		::print_counters(evlist);
}
