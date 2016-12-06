#include <cmath>
#include <iostream>
#include <tuple>

#include <cxx-prettyprint/prettyprint.hpp>
#include <fmt/format.h>

#include "cat-policy.hpp"
#include "log.hpp"


namespace cat
{
namespace policy
{


using fmt::literals::operator""_format;


void Slowfirst::set_masks(const std::vector<uint64_t> &masks)
{
	this->masks = masks;
	for (uint32_t i = 0; i < masks.size(); i++)
		cat.set_cos_mask(i, masks[i]);
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


void Slowfirst::apply(uint64_t current_interval, const std::vector<Task> &tasklist)
{
	// It's important to NOT make distinctions between completed and not completed tasks...
	// We asume that the event we care about has been programed as ev2.

	// Apply only when the amount of intervals specified has passed
	if (current_interval % every != 0)
		return;

	LOGDEB("function: {}, interval: {}"_format(__PRETTY_FUNCTION__, current_interval));

	// (Core, Combined stalls) tuple
	typedef std::tuple<uint32_t, uint64_t> pair;
	auto v = std::vector<pair>();

	cat.reset();
	check_masks(masks);
	set_masks(masks);

	for (uint32_t t = 0; t < tasklist.size(); t++)
	{
		const Task &task = tasklist[t];
		uint64_t l2_miss_stalls = task.stats_total.event[2];
		v.push_back(std::make_pair(task.cpu, l2_miss_stalls));
	}

	// Sort in descending order by stalls
	std::sort(begin(v), end(v),
			[](const pair &t1, const pair &t2)
			{
				return std::get<1>(t1) > std::get<1>(t2);
			});

	// Assign the slowest cores to masks which will hopefully help them
	for (uint32_t pos = 0; pos < masks.size() - 1; pos++)
	{
		uint32_t cos  = masks.size() - pos - 1;
		uint32_t core = std::get<0>(v[pos]);
		cat.set_cos_cpu(cos, core); // Assign core to COS
	}
}


void SlowfirstClustered::apply(uint64_t current_interval, const std::vector<Task> &tasklist)
{
	// Apply the policy only when the amount of intervals specified has passed
	if (current_interval % every != 0)
		return;

	assert(num_clusters > 0);

	auto data = std::vector<Point>();
	clusters.clear();

	cat.reset();
	set_masks(masks);

	LOGDEB(fmt::format("function: {}, interval: {}", __PRETTY_FUNCTION__, current_interval));

	// Put data in the format KMeans expects
	LOGDEB("Tasks:");
	for (uint32_t t = 0; t < tasklist.size(); t++)
	{
		const Task &task = tasklist[t];
		uint64_t l2_miss_stalls = task.stats_total.event[2];
		data.push_back(Point(t, {(double) l2_miss_stalls}));
		LOGDEB(fmt::format("{{id: {}, executable: {}, completed: {}, stalls: {:n}}}", task.id, task.executable, task.completed, l2_miss_stalls));
	}

	LOGDEB(fmt::format("Clusterize: {} points in {} clusters", data.size(), num_clusters));
	KMeans::clusterize(num_clusters, data, clusters, 100);

	// Sort clusters in descending order
	std::sort(begin(clusters), end(clusters),
			[](const Cluster &c1, const Cluster &c2)
			{
				return c1.getCentroid()[0] > c2.getCentroid()[0];
			});

	LOGDEB("Sorted clusters:");
	for (const auto &cluster : clusters)
		LOGDEB(cluster.to_string());

	// Iterate clusters
	LOGDEB("Classes Of Service:");
	for (size_t c = 0; c < clusters.size(); c++)
	{
		size_t cos  = masks.size() - c - 1;
		const auto &cluster = clusters[c];

		// Iterate tasks in cluster and put them in the adequate COS
		std::string task_ids;
		size_t i = 0;
		for(const auto &p : cluster.getPoints())
		{
			const Task &task = tasklist[p->id];
			cat.set_cos_cpu(cos, task.cpu);
			task_ids += i < cluster.getPoints().size() - 1 ?
					std::to_string(task.id) + ", ":
					std::to_string(task.id);
			i++;
		}

		LOGDEB(fmt::format("{{COS{}: {{mask: {:#x}, tasks: [{}]}}}}", cos, masks[cos], task_ids));
	}
}


void SlowfirstClusteredAdjusted::apply(uint64_t current_interval, const std::vector<Task> &tasklist)
{
	// Call parent first for clustering the applications
	SlowfirstClustered::apply(current_interval, tasklist);

	LOGDEB(fmt::format("function: {}, interval: {}", __PRETTY_FUNCTION__, current_interval));

	auto diffs = std::vector<uint64_t>(clusters.size());
	auto linear = std::vector<double>(clusters.size());
	auto quadratic = std::vector<double>(clusters.size());
	auto exponential = std::vector<double>(clusters.size());

	for (size_t i = 0; i < clusters.size(); i++)
	{
		diffs[i] = clusters[i].getCentroid()[0];
		if (i < clusters.size() - 1)
			diffs[i] -= clusters[i + 1].getCentroid()[0];

		linear[i] = diffs[i] / clusters[0].getCentroid()[0];
		quadratic[i] = pow(linear[i], 2);
		exponential[i] = exp(linear[i]);
	}

	LOGDEB("Intercluster distances:");
	LOGDEB(diffs);

	double ltot = 0;
	double qtot = 0;
	double etot = 0;
	for (double &x : linear) ltot += x;
	for (double &x : quadratic) qtot += x;
	for (double &x : exponential) etot += x;

	LOGDEB("Lineal, quadratic and exponential models:");
	LOGDEB(linear);
	LOGDEB(quadratic);
	LOGDEB(exponential);

	LOGDEB("Teoretical ways:");
	for (double &x : linear) x = x / ltot * max_num_ways;
	for (double &x : quadratic) x = x / qtot * max_num_ways;
	for (double &x : exponential) x = x / etot * max_num_ways;
	LOGDEB(linear);
	LOGDEB(quadratic);
	LOGDEB(exponential);

	for (auto v : {&linear, &quadratic, &exponential})
	{
		double p = max_num_ways;
		for (size_t i = 0; i < v->size(); i++)
		{
			auto &x = (*v)[i];
			x = p - x;
			p = x;
		}
		for (size_t i = v->size() - 1; i > 0; i--)
			(*v)[i] = std::max((uint32_t) round((*v)[i - 1]), cat::min_num_ways);
		(*v)[0] = max_num_ways;
	}

	LOGDEB("Effective ways:");
	LOGDEB(linear);
	LOGDEB(quadratic);
	LOGDEB(exponential);

	for (size_t i = 0;  i < clusters.size(); i++)
	{
		size_t cos = masks.size() - i - 1;
		uint32_t ways = exponential[i];
		masks[cos] = (complete_mask >> (max_num_ways - ways));
	}

	LOGDEB("Classes Of Service:");
	for (size_t cos = masks.size() - 1; cos < masks.size(); cos--)
		LOGDEB(fmt::format("{{COS{}: {{mask: {:#x}, num_ways: {}}}}}", cos, masks[cos], __builtin_popcount(masks[cos])));

	set_masks(masks);
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
				throw std::runtime_error("The 'none' model is not suposed to be called");
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
		throw std::runtime_error("Unwnown model '{}'"_format(name));

	model = models[name];
}


void SlowfirstClusteredOptimallyAdjusted::apply(uint64_t current_interval, const std::vector<Task> &tasklist)
{
	// Apply the policy only when the amount of intervals specified has passed
	if (current_interval % every != 0)
		return;

	auto data = std::vector<Point>();

	LOGDEB(fmt::format("function: {}, interval: {}", __PRETTY_FUNCTION__, current_interval));

	// Put data in the format KMeans expects
	LOGDEB("Tasks:");
	for (uint32_t t = 0; t < tasklist.size(); t++)
	{
		const Task &task = tasklist[t];
		uint64_t l2_miss_stalls = task.stats_total.event[2];
		data.push_back(Point(task.id, {(double) l2_miss_stalls}));
		LOGDEB(fmt::format("{{id: {}, executable: {}, completed: {}, stalls: {:n}}}", task.id, task.executable, task.completed, l2_miss_stalls));
	}

	if (num_clusters > 0)
	{
		LOGDEB("Enforce {} clusters..."_format(num_clusters));
		KMeans::clusterize(num_clusters, data, clusters, 100);
	}
	else
	{
		assert(num_clusters == 0);
		LOGDEB("Try to find the optimal number of clusters...");
		KMeans::clusterize_optimally(cat.get_max_num_cos(), data, clusters, 100);
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

	// Map clusters to COSs
	for (size_t c = 0; c < clusters.size(); c++)
	{
		const auto &cluster = clusters[c];

		// Iterate tasks in cluster and put them in the adequate COS
		size_t i = 0;
		for(const auto &p : cluster.getPoints())
		{
			const Task &task = tasklist[p->id];
			cat.set_cos_cpu(c, task.cpu);
			i++;
		}
	}

	// Show all the models
	for (const auto m : {Model("linear"), Model("quadratic"), Model("exponential")})
	{
		LOGDEB("The {} model:"_format(m.name));
		for (size_t i = 0; i < clusters.size(); i++)
		{
			const double x = clusters[i].getCentroid()[0] / clusters.front().getCentroid()[0];
			const double y = m(x);
			LOGDEB("Cluster {} : x = {} y = {} -> {} ways"_format(i, x, y, std::round(y)));
		}
	}
	LOGDEB("Selected model: {}"_format(model.name));

	// Get current CAT masks
	masks.resize(cat.get_max_num_cos());
	for (size_t cos = 0; cos < masks.size(); cos++)
		masks[cos] = cat.get_cos_mask(cos);

	// Compute new masks
	if (model.name != "none")
	{
		size_t c;
		for (c = 0;  c < clusters.size(); c++)
		{
			const double x = clusters[c].getCentroid()[0] / clusters.front().getCentroid()[0];
			const uint32_t ways = std::round(model(x));
			if (alternate_sides && (c % 2) == 1)
				masks[c] = (complete_mask << (max_num_ways - ways)) & complete_mask; // The mask starts from the left
			else
				masks[c] = (complete_mask >> (max_num_ways - ways)); // The mask starts from the right
		}
		for (;  c < masks.size(); c++)
			masks[c] = complete_mask;
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
				task_ids += std::to_string(p->id);
				task_ids += (i == clusters[c].getPoints().size() - 1) ? "" : ", ";
				i++;
			}
		}
		LOGDEB(fmt::format("{{COS{}: {{mask: {:#7x}, num_ways: {:2}, tasks: [{}]}}}}", c, masks[c], __builtin_popcount(masks[c]), task_ids));
	}

	if (model.name != "none")
		set_masks(masks);
}


}} // cat::policy
