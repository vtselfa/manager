#include <functional>
#include <iomanip>
#include <sstream>

#include <boost/io/ios_state.hpp>
#include <fmt/format.h>

#include "log.hpp"
#include "stats.hpp"
#include "throw-with-trace.hpp"


#define WIN_SIZE 7


namespace acc = boost::accumulators;

using fmt::literals::operator""_format;


Stats::Stats(const std::vector<std::string> &stats_names)
{
	init(stats_names);
}


void Stats::init_derived_metrics_total(const std::vector<std::string> &stats_names)
{
	bool instructions = std::find(stats_names.begin(), stats_names.end(), "instructions") != stats_names.end();
	bool cycles = std::find(stats_names.begin(), stats_names.end(), "cycles") != stats_names.end();
	bool ref_cycles = std::find(stats_names.begin(), stats_names.end(), "ref-cycles") != stats_names.end();

	if (instructions && cycles)
	{
		derived_metrics_total.push_back(std::make_pair("ipc", [this]()
		{
			double inst = this->sum("instructions");
			double cycl = this->sum("cycles");
			return inst / cycl;
		}));
	}

	if (instructions && ref_cycles)
	{
		derived_metrics_total.push_back(std::make_pair("ref-ipc", [this]()
		{
			double inst = this->sum("instructions");
			double ref_cycl = this->sum("ref-cycles");
			return inst / ref_cycl;
		}));
	}
}


void Stats::init_derived_metrics_int(const std::vector<std::string> &stats_names)
{
	bool instructions = std::find(stats_names.begin(), stats_names.end(), "instructions") != stats_names.end();
	bool cycles = std::find(stats_names.begin(), stats_names.end(), "cycles") != stats_names.end();
	bool ref_cycles = std::find(stats_names.begin(), stats_names.end(), "ref-cycles") != stats_names.end();
	if (instructions && cycles)
	{
		derived_metrics_int.push_back(std::make_pair("ipc", [this]()
		{
			double inst = this->last("instructions");
			double cycl = this->last("cycles");
			return inst / cycl;
		}));
	}

	if (instructions && ref_cycles)
	{
		derived_metrics_int.push_back(std::make_pair("ref-ipc", [this]()
		{
			double inst = this->last("instructions");
			double ref_cycl = this->last("ref-cycles");
			return inst / ref_cycl;
		}));
	}
}


void Stats::init(const std::vector<std::string> &stats_names)
{
	assert(!initialized);

	for (const auto &c : stats_names)
		events.insert(std::make_pair(c, accum_t(acc::tag::rolling_window::window_size = WIN_SIZE)));

	init_derived_metrics_int(stats_names);
	init_derived_metrics_total(stats_names);

	// Test der metrics
	if (derived_metrics_int.size() != derived_metrics_total.size())
		throw_with_trace(std::runtime_error("Different number of derived metrics for int ({}) and total ({})"_format(
				derived_metrics_int.size(), derived_metrics_total.size())));
	for(const auto &der : derived_metrics_int)
	{
		auto it = std::find_if(derived_metrics_total.begin(), derived_metrics_total.end(),
				[&der](const auto& tuple) {return tuple.first == der.first;});
		if (it == derived_metrics_total.end())
			throw_with_trace(std::runtime_error("Different derived metrics for int and total results"));
	}

	for (const auto &der : derived_metrics_int)
		events.insert(std::make_pair(der.first, accum_t(acc::tag::rolling_window::window_size = WIN_SIZE)));

	// Store the names of the counters
	names = stats_names;

	initialized = true;
}


Stats& Stats::accum(const counters_t &counters)
{
	assert(initialized);

	clast = ccurr;
	ccurr = counters;

	const auto &curr_id_idx = ccurr.get<by_id>();
	const auto &last_id_idx = clast.get<by_id>();

	assert(!ccurr.empty());

	// App has just started, no last data
	if (clast.empty())
	{
		auto it = curr_id_idx.cbegin();
		while (it != curr_id_idx.cend())
		{
			double value = (it->name == "power/energy-ram/" || it->name == "power/energy-pkg/") ?
					0 :
					it->value;

			assert(it->enabled > 0 && it->enabled <= 1);
			if (it->enabled < 1)
			{
				value /= it->enabled;
				LOGINF("Counter {} has been scaled ({})"_format(it->name, it->enabled));
			}

			events.at(it->name)(value);
			it++;
		}
	}

	// We have data from the last interval
	else
	{
		assert(ccurr.size() == clast.size());
		auto curr_it = curr_id_idx.cbegin();
		auto last_it = last_id_idx.cbegin();
		while (curr_it != curr_id_idx.cend() && last_it != last_id_idx.cend())
		{
			const Counter &c = *curr_it;
			const Counter &l = *last_it;
			assert(c.id == l.id);
			assert(c.name == l.name);
			double value = c.snapshot ?
					c.value :
					c.value - l.value;

			if (value < 0)
				LOGERR("Negative interval value ({}) for the counter '{}'"_format(value, c.name));

			assert(c.enabled > 0 && c.enabled <= 1);
			if (c.enabled < 1)
			{
				value /= c.enabled;
				LOGINF("Counter {} has been scaled ({})"_format(c.name, c.enabled));
			}

			events.at(c.name)(value);

			curr_it++;
			last_it++;
		}
	}

	// Compute and add derived metrics
	for (const auto &der : derived_metrics_int)
		events.at(der.first)(der.second());

	counter++;

	return *this;
}


std::string Stats::header_to_string(const std::string &sep) const
{
	if (!names.size()) return "";

	std::stringstream ss;
	auto it = names.begin();
	ss << *it;
	it++;
	for (; it != names.end(); it++)
		ss << sep << *it;
	for (const auto &der : derived_metrics_int) // Int, snapshot and total have the same derived metrics
		ss << sep << der.first;
	return ss.str();
}


// Some of the counters are collected as snapshots of the state of the system (i.e. the cache space occupation).
// This is taken into account to compute the value of the metric for the interval. If force_snapshot is true, then
// the function prints the value of the counter. If not, it prints the difference with the previous interval, unless
// the metric is collected as an snapshot.
std::string Stats::data_to_string_total(const std::string &sep) const
{
	std::stringstream ss;

	assert(ccurr.size() > 0);

	const auto &curr_id_idx = ccurr.get<by_id>();
	auto it = curr_id_idx.cbegin();
	while (it != curr_id_idx.cend())
	{
		const auto &name = it->name;
		const accum_t &event = events.at(name);
		double value = it->snapshot ?
				acc::mean(event) :
				acc::sum(event);
		ss << value;
		it++;
		if (it != curr_id_idx.cend())
			ss << sep;
	}

	// Derived metrics
	for (auto it1 = derived_metrics_total.cbegin(); it1 != derived_metrics_total.cend(); it1++)
	{
		double value = it1->second();
		ss << sep << value;
	}

	return ss.str();
}


std::string Stats::data_to_string_int(const std::string &sep) const
{
	std::stringstream ss;

	assert(ccurr.size() > 0);

	const auto &curr_id_idx = ccurr.get<by_id>();

	auto curr_it = curr_id_idx.cbegin();
	while (curr_it != curr_id_idx.cend())
	{
		ss << acc::last(events.at(curr_it->name));
		curr_it++;

		if (curr_it != ccurr.cend())
			ss << sep;
	}

	// Derived metrics
	for (auto it = derived_metrics_int.cbegin(); it != derived_metrics_int.cend(); it++)
	{
		double value = it->second();
		ss << sep << value;
	}

	return ss.str();
}


double Stats::get_current(const std::string &name) const
{
	const auto &curr_index = ccurr.get<by_name>();
	auto it = curr_index.find(name);
	if (it == curr_index.end())
		throw_with_trace(std::runtime_error("Event not monitorized '{}'"_format(name)));
	return it->value / it->enabled;
}


double Stats::sum(const std::string &name) const
{
	return acc::sum(events.at(name));
}


double Stats::last(const std::string &name) const
{
	return acc::last(events.at(name));
}


void Stats::reset_counters()
{
	clast = counters_t();
	ccurr = counters_t();
}
