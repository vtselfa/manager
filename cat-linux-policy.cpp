#include <cmath>
#include <iostream>
#include <iterator>
#include <tuple>

#include <fmt/format.h>

#include "cat-linux-policy.hpp"
#include "log.hpp"


namespace cat
{
namespace policy
{


namespace acc = boost::accumulators;
using fmt::literals::operator""_format;


void SFL::set_masks(const std::vector<uint64_t> &masks)
{
	this->masks = masks;
	for (uint32_t i = 0; i < masks.size(); i++)
		get_cat()->set_cbm(i, masks[i]);
}


void SFL::check_masks(const std::vector<uint64_t> &masks) const
{
	uint64_t last_mask = 0;
	std::string m;
	for (auto mask : masks)
	{
		if (last_mask > mask)
		{
			LOGWAR("The masks for the slowfirst CAT policy may be in the wrong order");
			break;
		}
		last_mask = mask;
		m += std::to_string(mask) + " ";
	}
	LOGDEB("Slowfirst masks: " << m);
}


void SFL::apply(uint64_t current_interval, const std::vector<Task> &tasklist)
{
	// Apply only when the amount of intervals specified has passed
	if (current_interval % every != 0)
		return;

	LOGDEB("function: {}, interval: {}"_format(__PRETTY_FUNCTION__, current_interval));

	// (Core, Combined stalls) tuple
	typedef std::tuple<pid_t, uint64_t> pair_t;
	auto v = std::vector<pair_t>();

	check_masks(masks);
	set_masks(masks);

	for (uint32_t t = 0; t < tasklist.size(); t++)
	{
		uint64_t l2_miss_stalls;
		const Task &task = tasklist[t];
		try
		{
			l2_miss_stalls = acc::sum(task.stats_total.events.at("CYCLE_ACTIVITY.STALLS_TOTAL"));
		}
		catch (const std::exception &e)
		{
			std::string msg = "This policy requires the event CYCLE_ACTIVITY.STALLS_TOTAL. The events monitorized are:";
			for (const auto &kv : task.stats_total.events)
				msg += "\n" + kv.first;
			throw_with_trace(std::runtime_error(msg));
		}
		v.push_back(std::make_pair(task.pid, l2_miss_stalls));
	}

	// Sort in descending order by stalls
	std::sort(begin(v), end(v),
			[](const pair_t &t1, const pair_t &t2)
			{
				return std::get<1>(t1) > std::get<1>(t2);
			});

	// Assign the slowest pids to masks which will hopefully help them
	for (uint32_t pos = 0; pos < masks.size() - 1 && pos < tasklist.size(); pos++)
	{
		uint32_t cos  = masks.size() - pos - 1;
		pid_t pid = std::get<0>(v[pos]);
		get_cat()->add_task(cos, pid);
	}
}


}} // cat::policy
