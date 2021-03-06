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


Stats::Stats(const std::vector<std::string> &counters)
{
	init(counters);
}


void Stats::init_derived_metrics_total(const std::vector<std::string> &counters)
{
	bool instructions = std::find(counters.begin(), counters.end(), "instructions") != counters.end();
	bool cycles = std::find(counters.begin(), counters.end(), "cycles") != counters.end();
	bool ref_cycles = std::find(counters.begin(), counters.end(), "ref-cycles") != counters.end();

	if (instructions && cycles)
	{
		derived_metrics_total.push_back(std::make_pair("ipc", [this]()
		{
			double inst = acc::sum(this->events.at("instructions"));
			double cycl = acc::sum(this->events.at("cycles"));
			return inst / cycl;
		}));
	}

	if (instructions && ref_cycles)
	{
		derived_metrics_total.push_back(std::make_pair("ref-ipc", [this]()
		{
			double inst = acc::sum(this->events.at("instructions"));
			double ref_cycl = acc::sum(this->events.at("ref-cycles"));
			return inst / ref_cycl;
		}));
	}
}


void Stats::init_derived_metrics_int(const std::vector<std::string> &counters)
{
	bool instructions = std::find(counters.begin(), counters.end(), "instructions") != counters.end();
	bool cycles = std::find(counters.begin(), counters.end(), "cycles") != counters.end();
	bool ref_cycles = std::find(counters.begin(), counters.end(), "ref-cycles") != counters.end();
	if (instructions && cycles)
	{
		derived_metrics_int.push_back(std::make_pair("ipc", [this]()
		{
			double inst = this->get_interval("instructions");
			double cycl = this->get_interval("cycles");
			return inst / cycl;
		}));
	}

	if (instructions && ref_cycles)
	{
		derived_metrics_int.push_back(std::make_pair("ref-ipc", [this]()
		{
			double inst = this->get_interval("instructions");
			double ref_cycl = this->get_interval("ref-cycles");
			return inst / ref_cycl;
		}));
	}
}


void Stats::init(const std::vector<std::string> &counters)
{
	assert(!initialized);

	for (const auto &c : counters)
		events.insert(std::make_pair(c, accum_t(acc::tag::rolling_window::window_size = WIN_SIZE)));

	init_derived_metrics_int(counters);
	init_derived_metrics_total(counters);

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
	names = counters;

	initialized = true;
}


Stats& Stats::accum(const counters_t &counters)
{
	assert(initialized);

	last = curr;
	curr = counters;

	const auto &curr_id_idx = curr.get<by_id>();
	const auto &last_id_idx = last.get<by_id>();

	assert(!curr.empty());

	// App has just started, no last data
	if (last.empty())
	{
		auto it = curr_id_idx.cbegin();
		while (it != curr_id_idx.cend())
		{
			events.at(it->name)(it->value);
			it++;
		}
	}

	// We have data from the last interval
	else
	{
		assert(curr.size() == last.size());
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

	assert(curr.size() > 0);

	const auto &name_index = curr.get<by_name>();
	for (size_t i = 0; i < names.size(); i++)
	{
		const auto &name = names[i];
		const accum_t &event = events.at(name);
		const auto &c = name_index.find(name);
		double value = c->snapshot ?
				acc::mean(event) :
				acc::sum(event);
		ss << value;
		if (i < names.size() - 1)
			ss << sep;
	}

	// Derived metrics
	for (auto it = derived_metrics_total.cbegin(); it != derived_metrics_total.cend(); it++)
	{
		double value = it->second();
		ss << sep << value;
	}

	return ss.str();
}


std::string Stats::data_to_string_int(const std::string &sep) const
{
	std::stringstream ss;

	assert(curr.size() > 0);

	const auto &curr_id_idx = curr.get<by_id>();

	auto curr_it = curr_id_idx.cbegin();
	while (curr_it != curr_id_idx.cend())
	{
		ss << acc::last(events.at(curr_it->name));
		curr_it++;

		if (curr_it != curr.cend())
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


double Stats::get_interval(const std::string &name) const
{
	const auto &curr_index = curr.get<by_name>();
	const auto &last_index = last.get<by_name>();

	const auto c = curr_index.find(name);
	const auto l = last_index.find(name);


	if (last.size() != curr.size() && last.size() != 0)
		throw_with_trace(std::runtime_error("Inconsistency between current data and last interval data"));

	if (c == curr_index.end())
		throw_with_trace(std::runtime_error("Missing current data"));

	double value = c->snapshot || last.size() == 0 ?
			c->value :
			c->value - l->value;
	return value;
}


double Stats::get_current(const std::string &name) const
{
	const auto &curr_index = curr.get<by_name>();
	auto it = curr_index.find(name);
	if (it == curr_index.end())
		throw_with_trace(std::runtime_error("Event not monitorized '{}'"_format(name)));
	return it->value;
}


double Stats::sum(const std::string &name) const
{
	return acc::sum(events.at(name));
}


const counters_t& Stats::get_current_counters() const
{
	return curr;
}


void Stats::reset_counters()
{
	last = counters_t();
	curr = counters_t();
}
