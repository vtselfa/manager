#include <cmath>
#include <iostream>
#include <iterator>
#include <tuple>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/max.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/variance.hpp>
#include <cxx-prettyprint/prettyprint.hpp>
#include <fmt/format.h>

#include "cat-policy.hpp"
#include "log.hpp"


namespace cat
{
namespace policy
{


namespace acc = boost::accumulators;
using fmt::literals::operator""_format;


void Slowfirst::set_masks(const std::vector<uint64_t> &masks)
{
	this->masks = masks;
	for (uint32_t i = 0; i < masks.size(); i++)
		cat->set_cbm(i, masks[i]);
}


void Slowfirst::check_masks(const std::vector<uint64_t> &masks) const
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


// We want to slit an area of size max_num_ways in clusters.size() partitions.
// This partitions have a minimum size of min_num_ways ans a maximum size of max_num_ways.
// We will use 3 different models: a linear, a quadratic, and an exponential model.
// Therefore we will have 3 different functions (fl, fq fe), defined for the intervals [0, al], [0, aq], and [0, ae].
// Each one fulfills fn(0) == min_num_ways and fn(an) == max_num_ways and grows linearli, quadratically and exponentially, respectively.
SfCOA::Model::Model(const std::string &name) : name(name)
{
	models =
	{
		{ "none", [](double x) -> double
			{
				assert(x >= 0 && x <= 1);
				throw_with_trace(std::runtime_error("The 'none' model is not suposed to be called"));
			}
		},
		{ "linear", [](double x) -> double
			{
				assert(x >= 0 && x <= 1);
				const double a = max_num_ways - min_num_ways;
				x *= a; // Scale X for the interval [0, a]
				return x + min_num_ways;
			}
		},
		{ "quadratic", [](double x) -> double
			{
				assert(x >= 0 && x <= 1);
				const double a = std::sqrt(max_num_ways - min_num_ways);
				x *= a; // Scale X for the interval [0, a]
				return std::pow(x, 2) + min_num_ways;
			}
		},
		{ "exponential", [](double x) -> double
			{
				assert(x >= 0 && x <= 1);
				const double a = std::log(max_num_ways - min_num_ways + 1);
				x *= a; // Scale X for the interval [0, a]
				return std::exp(x) + min_num_ways - 1;
			}
		},
		{ "expquad", [](double x) -> double
			{
				assert(x >= 0 && x <= 1);
				const double a = std::sqrt(std::log(max_num_ways - min_num_ways + 1));
				x *= a; // Scale X for the interval [0, a]
				return std::exp(pow(x, 2)) + min_num_ways - 1;
			}
		},
		{ "log", [](double x) -> double
			{
				assert(x >= 0 && x <= 1);
				const double a = 14.849;
				x *= a; // Scale X for the interval [0, a]
				return 15 * std::log(x + 1) + min_num_ways;
			}
		},
		{ "linlog", [](double x) -> double
			{
				assert(x >= 0 && x <= 1);
				const double a = 15.222;
				x *= a; // Scale X for the interval [0, a]
				return x * std::log(x) + 2;
			}
		},
		{ "camel", [](double x) -> double
			{
				assert(x >= 0 && x <= 1);
				const double a = 21.522;
				x *= a; // Scale X for the interval [0, a]
				return (0.9 * x - 25) * std::exp(0.1 * x) + 0.005 * std::pow(x + 40, 2) + x + 24;
			}
		},
	};

	if (models.count(name) == 0)
		throw_with_trace(std::runtime_error("Unwnown model '{}'"_format(name)));

	model = models[name];
}


void SlowfirstClusteredOptimallyAdjusted::apply(uint64_t current_interval, const std::vector<Task> &tasklist)
{
	// Apply the policy only when the amount of intervals specified has passed
	if (current_interval % every != 0)
		return;

	auto data = points_t();
	acc::accumulator_set<double, acc::stats<acc::tag::mean, acc::tag::variance, acc::tag::max>> accum;

	LOGDEB(fmt::format("function: {}, interval: {}", __PRETTY_FUNCTION__, current_interval));

	// Put data in the format KMeans expects
	LOGDEB("Tasks:");
	for (const auto &task : tasklist)
	{
		uint64_t l3_hits;
		uint64_t l3_misses;
		uint64_t stalls;
		uint64_t accum_stalls;
		const std::string he = "MEM_LOAD_UOPS_RETIRED.L3_HIT";
		const std::string me = "MEM_LOAD_UOPS_RETIRED.L3_MISS";
		const std::string se = "CYCLE_ACTIVITY.STALLS_TOTAL";
		const Stats &stats = task.stats;
		try
		{
			l3_hits = acc::rolling_mean(stats.events.at(he));
			l3_misses = acc::rolling_mean(stats.events.at(me));
			stalls = acc::rolling_mean(stats.events.at(se));
			accum_stalls = acc::sum(stats.events.at(se));
		}
		catch (const std::exception &e)
		{
			std::string msg = "This policy requires the events {}, {} and {}. The events monitorized are:"_format(he, me, se);
			for (const auto &kv : stats.events)
				msg += "\n" + kv.first;
			throw_with_trace(std::runtime_error(msg));
		}

		double hr = (double) l3_hits / (double) (l3_hits + l3_misses);
		double metric = accum_stalls;

		double completed = task.max_instr ?
				(double) stats.get_current("instructions") / (double) task.max_instr : task.completed;

		accum(stalls);

		data.push_back(std::make_shared<Point>(task.id, std::vector<double>(metric)));
		LOGDEB(fmt::format("{{id: {}, name: {}, completed: {:.2f}, metric: {}, stalls: {:n}, hr: {}}}", task.id, task.name, completed, metric, stalls, hr));
	}

	if (min_stall_ratio > 0)
	{
		// Ratio of cycles stalled and cycles the execution has been running for the most stalled application
		const double stall_ratio = acc::max(accum) / tasklist[0].stats.get_current("ref_cycles");
		if (stall_ratio < min_stall_ratio)
		{
			LOGDEB("Better to do nothing, since the processor is only stalled {}% of the time"_format(stall_ratio * 100));
			cat->reset();
			return;
		}
	}

	if (num_clusters > 0)
	{
		LOGDEB("Enforce {} clusters..."_format(num_clusters));
		KMeans::clusterize(num_clusters, data, clusters, 100);
	}
	else
	{
		assert(num_clusters == 0);

		if (cluster_sizes.size() > 0)
		{
			LOGDEB("Use predetermined cluster sizes...");
			// Sort points in DESCENDING order
			std::sort(begin(data), end(data),
					[](const point_ptr_t p1, const point_ptr_t p2)
					{
						return p1->values[0] > p2->values[0];
					});
			clusters.clear();
			size_t cid = 0;
			auto data_iter = data.begin();
			for (const auto &size : cluster_sizes)
			{
				clusters.push_back(Cluster(cid++, {0}));
				for (size_t i = 0; i < size; i++)
				{
					assert(data_iter != data.end());
					const auto point = *data_iter;
					clusters.back().addPoint(point);
					data_iter++;
				}
				clusters.back().updateMeans();
			}
		}
		else if (min_max)
		{
			LOGDEB("Make min max pairs...");
			assert(data.size() % 2 == 0 && (data.size() / 2) <= cat->get_max_closids());
			// Sort points in DESCENDING order
			std::sort(begin(data), end(data),
					[](const point_ptr_t p1, const point_ptr_t p2)
					{
						return p1->values[0] > p2->values[0];
					});
			clusters.clear();
			size_t cid = 0;
			auto it1 = data.begin();
			auto it2 = data.end();
			it2--;
			while (std::distance(data.begin(), it1) < std::distance(data.begin(), it2))
			{
				const auto p1 = *it1;
				const auto p2 = *it2;
				clusters.push_back(Cluster(cid++, {0}));
				clusters.back().addPoint(p1);
				clusters.back().addPoint(p2);
				clusters.back().updateMeans();
				it1++;
				it2--;
			}
		}
		else
		{
			LOGDEB("Try to find the optimal number of clusters...");
			KMeans::clusterize_optimally(cat->get_max_closids(), data, clusters, 100, eval_clusters);
		}
	}

	LOGDEB(fmt::format("Clusterize: {} points in {} clusters", data.size(), clusters.size()));

	// Sort clusters in DESCENDING order
	std::sort(begin(clusters), end(clusters),
			[](const Cluster &c1, const Cluster &c2)
			{
				return c1.getCentroid()[0] > c2.getCentroid()[0];
			});

	LOGDEB("Sorted clusters:");
	for (const auto &cluster : clusters)
		LOGDEB(cluster.to_string());

	LOGDEB("Selected model: {}"_format(model.name));
	if (model.name != "none")
	{
		for (size_t i = 0; i < clusters.size(); i++)
		{
			const double x = clusters[i].getCentroid()[0] / clusters.front().getCentroid()[0];
			const double y = model(x);
			LOGDEB("Cluster {} : x = {} y = {} -> {} ways"_format(i, x, y, std::round(y)));
		}
	}

	// Get current CAT masks
	masks.resize(cat->get_max_closids());
	for (size_t cos = 0; cos < masks.size(); cos++)
		masks[cos] = cat->get_cbm(cos);

	// Compute new masks
	if (model.name != "none")
	{
		const double th = acc::mean(accum) + 2 * std::sqrt(acc::variance(accum));
		const point_ptr_t p = *std::max_element(clusters[1].getPoints().begin(), clusters[1].getPoints().end(), [](const point_ptr_t a, const point_ptr_t b){ return a->values[0] < b->values[0]; });

		size_t c;
		for (c = 0;  c < clusters.size(); c++)
		{
			double quotient = clusters.front().getCentroid()[0];
			if (detect_outliers && quotient > th && clusters[0].getPoints().size() == 1)
			{
				quotient = p->values[0]; // The max in the second cluster
				LOGDEB("The most slowed-down application seems an outlayer... use {} instead of {}, {}"_format(quotient, clusters.front().getCentroid()[0], quotient / clusters.front().getCentroid()[0]));
			}

			const double x = std::min(clusters[c].getCentroid()[0] / quotient, 1.0);
			const uint32_t ways = std::round(model(x));
			if (alternate_sides && (c % 2) == 1)
				masks[c] = (complete_mask << (max_num_ways - ways)) & complete_mask; // The mask starts from the left
			else
				masks[c] = (complete_mask >> (max_num_ways - ways)); // The mask starts from the right
		}
		for (;  c < masks.size(); c++)
			masks[c] = complete_mask;
		set_masks(masks);
	}

	LOGDEB("Classes Of Service:");
	for (size_t c = 0; c < masks.size(); c++)
	{
		std::string task_ids;
		if (c < clusters.size())
		{
			size_t i = 0;
			for (const auto &p : clusters[c].getPoints())
			{
				const Task &task = tasklist[p->id];
				assert(task.cpus.size() == 1);
				cat->add_cpu(c, task.cpus.front());
				task_ids += std::to_string(p->id);
				task_ids += (i == clusters[c].getPoints().size() - 1) ? "" : ", ";
				i++;
			}
		}
		LOGDEB(fmt::format("{{COS{}: {{mask: {:#7x}, num_ways: {:2}, tasks: [{}]}}}}", c, masks[c], __builtin_popcount(masks[c]), task_ids));
	}
}


}} // cat::policy
