#include <algorithm>
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


// Assign each task to a cluster
clusters_t ClusteringBase::apply(const tasklist_t &tasklist)
{
	auto clusters = clusters_t();
	for (const auto &task : tasklist)
	{
		clusters.push_back(Cluster(task.id, {0}));
		clusters[task.id].addPoint(
				std::make_shared<Point>(task.id, std::vector<double>{0}));
	}
	return clusters;
}


clusters_t Cluster_SF::apply(const tasklist_t &tasklist)
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
			clusters[s].addPoint(std::make_shared<Point>(task_id, std::vector<double>{(double) task_stalls}));
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


clusters_t Cluster_KMeans::apply(const tasklist_t &tasklist)
{
	auto data = std::vector<point_ptr_t>();

	// Put data in the format KMeans expects
	for (const auto &task : tasklist)
	{
		double metric;
		try
		{
			metric = acc::rolling_mean(task.stats.events.at(event));
		}
		catch (const std::exception &e)
		{
			std::string msg = "This policy requires the event '{}'. The events monitorized are:"_format(event);
			for (const auto &kv : task.stats.events)
				msg += "\n" + kv.first;
			throw_with_trace(std::runtime_error(msg));
		}

		data.push_back(std::make_shared<Point>(task.id, std::vector<double>{metric}));
	}

	std::vector<Cluster> clusters;
	if (num_clusters > 0)
	{
		LOGDEB("Enforce {} clusters..."_format(num_clusters));
		KMeans::clusterize(num_clusters, data, clusters, 100);
	}
	else
	{
		assert(num_clusters == 0);

		LOGDEB("Try to find the optimal number of clusters...");
		KMeans::clusterize_optimally(max_clusters, data, clusters, 100, eval_clusters);
	}

	LOGDEB(fmt::format("Clusterize: {} points in {} clusters using the event '{}'", data.size(), clusters.size(), event));

	// Sort clusters in ASCENDING order
	if (sort_ascending)
	{
		std::sort(begin(clusters), end(clusters),
				[this](const Cluster &c1, const Cluster &c2)
				{
					return c1.getCentroid()[0] < c2.getCentroid()[0];
				});
	}

	// Sort clusters in DESCENDING order
	else
	{
		std::sort(begin(clusters), end(clusters),
				[this](const Cluster &c1, const Cluster &c2)
				{
					return c1.getCentroid()[0] > c2.getCentroid()[0];
				});
	}
	LOGDEB("Sorted clusters in {} order:"_format(sort_ascending ? "ascending" : "descending"));
	for (const auto &cluster : clusters)
		LOGDEB(cluster.to_string());

	return clusters;
}


ways_t Distribute_N::apply(const tasklist_t &, const clusters_t &clusters)
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


ways_t Distribute_RelFunc::apply(const tasklist_t &, const clusters_t &clusters)
{
	ways_t cbms;
	auto values = std::vector<double>();
	if (invert_metric)
		LOGDEB("Inverting metric...");
	for (size_t i = 0; i < clusters.size(); i++)
	{
		if (invert_metric)
			values.push_back(1 / clusters[i].getCentroid()[0]);
		else
			values.push_back(clusters[i].getCentroid()[0]);
	}
	double max = *std::max_element(values.begin(), values.end());

	for (size_t i = 0; i < values.size(); i++)
	{
		double x = values[i] / max;
		double y;
		assert(x >= 0 && x <= 1);
		const double a = std::log(max_ways - min_ways + 1);
		x *= a; // Scale X for the interval [0, a]
		y = std::exp(x) + min_ways - 1;
		int ways = std::round(y);
		cbms.push_back(cut_mask(~(-1 << ways)));

		LOGDEB("Cluster {} : x = {} y = {} -> {} ways"_format(i, x, y, ways));
	}
	return cbms;
}


void ClusterAndDistribute::show(const tasklist_t &tasklist, const clusters_t &clusters, const ways_t &ways)
{
	assert(clusters.size() == ways.size());
	for (size_t i = 0; i < ways.size(); i++)
	{
		std::string task_ids;
		const auto &points = clusters[i].getPoints();
		size_t p = 0;
		for (const auto &point : points)
		{
			const Task &task = tasklist[point->id];
			task_ids += "{}:{}"_format(task.id, task.name);
			task_ids += (p == points.size() - 1) ? "" : ", ";
			p++;
		}
		LOGDEB(fmt::format("{{COS{}: {{mask: {:#7x}, num_ways: {:2}, tasks: [{}]}}}}", i, ways[i], __builtin_popcount(ways[i]), task_ids));
	}
}

}} // cat::policy
