#include <cmath>
#include <iostream>
#include <iterator>
#include <memory>
#include <tuple>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/max.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/variance.hpp>
#include <fmt/format.h>

#include "cat-linux-policy.hpp"
#include "kmeans.hpp"
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
			l2_miss_stalls = acc::sum(task.stats.events.at("CYCLE_ACTIVITY.STALLS_TOTAL"));
		}
		catch (const std::exception &e)
		{
			std::string msg = "This policy requires the event CYCLE_ACTIVITY.STALLS_TOTAL. The events monitorized are:";
			for (const auto &kv : task.stats.events)
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


clusters_t Cluster_SF::apply(const std::vector<Task> &tasklist)
{
	// (Pos, Stalls) tuple
	typedef std::pair<pid_t, uint64_t> pair_t;
	auto v = std::vector<pair_t>();

	for (uint32_t t = 0; t < tasklist.size(); t++)
	{
		uint64_t stalls;
		const Task &task = tasklist[t];
		try
		{
			stalls = acc::sum(task.stats.events.at("cycle_activity.stalls_ldm_pending"));
		}
		catch (const std::exception &e)
		{
			std::string msg = "This policy requires the event 'cycle_activity.stalls_ldm_pending'. The events monitorized are:";
			for (const auto &kv : task.stats.events)
				msg += "\n" + kv.first;
			throw_with_trace(std::runtime_error(msg));
		}
		v.push_back(std::make_pair(t, stalls));
	}

	// Sort in descending order by stalls
	std::sort(begin(v), end(v),
			[](const pair_t &t1, const pair_t &t2)
			{
				return std::get<1>(t1) > std::get<1>(t2);
			});

	// Assign tasks to clusters
	auto clusters = clusters_t();
	size_t t = 0;
	for (size_t s = 0; s < sizes.size(); s++)
	{
		auto size = sizes[s];
		clusters.push_back(Cluster(s, {0}));
		assert(size > 0);
		while (size > 0)
		{
			auto task_id = v[t].first;
			auto task_stalls = v[t].second;
			// auto ptr = std::make_shared<Point>(11, std::vector<double>());
			clusters[s].addPoint(std::make_shared<Point>(task_id, std::vector<double>((double) task_stalls)));
			size--;
			t++;
		}
		clusters[s].updateMeans();
	}
	if (t != tasklist.size())
	{
		int diff = (int) t - (int) tasklist.size();
		throw_with_trace(std::runtime_error("This clustering policy expects {} {} tasks"_format(std::abs(diff), diff > 0 ? "more" : "less")));
	}
	return clusters;
}


clusters_t Cluster_KMeans::apply(const std::vector<Task> &tasklist)
{
	auto data = std::vector<point_ptr_t>();
	acc::accumulator_set<double, acc::stats<acc::tag::mean, acc::tag::variance, acc::tag::max>> accum;

	// Put data in the format KMeans expects
	for (const auto &task : tasklist)
	{
		uint64_t stalls;
		try
		{
			stalls = acc::sum(task.stats.events.at("cycle_activity.stalls_ldm_pending"));
		}
		catch (const std::exception &e)
		{
			std::string msg = "This policy requires the event 'cycle_activity.stalls_ldm_pending'. The events monitorized are:";
			for (const auto &kv : task.stats.events)
				msg += "\n" + kv.first;
			throw_with_trace(std::runtime_error(msg));
		}
		accum(stalls);

		data.push_back(std::make_shared<Point>(task.id, std::vector<double>((double) stalls)));
	}

	std::vector<Cluster> kmeans_clusters;
	if (num_clusters > 0)
	{
		LOGDEB("Enforce {} clusters..."_format(num_clusters));
		KMeans::clusterize(num_clusters, data, kmeans_clusters, 100);
	}
	else
	{
		assert(num_clusters == 0);

		LOGDEB("Try to find the optimal number of clusters...");
		KMeans::clusterize_optimally(max_clusters, data, kmeans_clusters, 100, eval_clusters);
	}

	LOGDEB(fmt::format("Clusterize: {} points in {} clusters", data.size(), kmeans_clusters.size()));

	return kmeans_clusters;
}


ways_t Distribute_N::apply(const std::vector<Task> &, clusters_t &clusters)
{
	ways_t ways(clusters.size(), -1);
	for (size_t i = 0; i < clusters.size(); i++)
	{
		ways[i] <<= ((i + 1) * n);
		ways[i] = cut_mask(ways[i]);
		if (ways[i] == 0)
			throw_with_trace(std::runtime_error("Too many CLOSes ({}) or N too big ({}) have resulted in an empty mask"_format(clusters.size(), n)));
	}
	return ways;
}


ways_t Distribute_RelFunc::apply(const std::vector<Task> &, clusters_t &clusters)
{
	// Sort clusters in DESCENDING order
	std::sort(begin(clusters), end(clusters),
			[](const Cluster &c1, const Cluster &c2)
			{
				return c1.getCentroid()[0] > c2.getCentroid()[0];
			});

	ways_t cbms(clusters.size(), -1);
	for (size_t i = 0; i < clusters.size(); i++)
	{
		double x = clusters[i].getCentroid()[0] / clusters.front().getCentroid()[0];
		double y;
		assert(x >= 0 && x <= 1);
		const double a = std::log(max_num_ways - min_num_ways + 1);
		x *= a; // Scale X for the interval [0, a]
		y = std::exp(x) + min_num_ways - 1;
		int ways = std::round(y);
		cbms[i] = cut_mask(~(-1 << ways));

		LOGDEB("Cluster {} : x = {} y = {} -> {} ways"_format(i, x, y, ways));
	}
	return cbms;
}

}} // cat::policy
