#include <algorithm>
#include <iostream>
#include <tuple>

#include <cxx-prettyprint/prettyprint.hpp>
#include <fmt/format.h>

#include "cat-policy.hpp"
#include "log.hpp"


void CAT_Policy_Slowfirst::set_masks(const std::vector<uint64_t> &masks)
{
	this->masks = masks;
	for (uint32_t i = 0; i < masks.size(); i++)
		cat.set_cos_mask(i, masks[i]);
}


void CAT_Policy_Slowfirst::check_masks(const std::vector<uint64_t> &masks) const
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


void CAT_Policy_Slowfirst::adjust(uint64_t current_interval, const std::vector<Task> &tasklist)
{
	// It's important to NOT make distinctions between completed and not completed tasks...
	// We asume that the event we care about has been programed as ev2.

	// Adjust only when the amount of intervals specified has passed
	if (current_interval % every != 0)
		return;

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


void CAT_Policy_SF_Kmeans::adjust(uint64_t current_interval, const std::vector<Task> &tasklist)
{
	// Adjust only when the amount of intervals specified has passed
	if (current_interval % every != 0)
		return;

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
		for(const auto &item : cluster.getPoints())
		{
			const size_t index = item.first;
			const Task &task = tasklist[index];
			cat.set_cos_cpu(cos, task.cpu);
			task_ids += i < cluster.getPoints().size() - 1 ?
					std::to_string(task.id) + ", ":
					std::to_string(task.id);
			i++;
		}

		LOGDEB(fmt::format("{{COS{}: {{mask: {:#x}, tasks: [{}]}}}}", cos, masks[cos], task_ids));
	}
}


void CAT_Policy_SF_Kmeans2::adjust(uint64_t current_interval, const std::vector<Task> &tasklist)
{
	using fmt::literals::operator""_format;

	// Call parent first
	CAT_Policy_SF_Kmeans::adjust(current_interval, tasklist);

	// Adjust only when the amount of intervals specified has passed
	if (current_interval % every_m != 0)
		return;

	LOGDEB(fmt::format("function: {}, interval: {}", __PRETTY_FUNCTION__, current_interval));

	auto diffs = std::vector<uint64_t>(clusters.size() - 1);
	auto &prev = clusters[0];
	for (size_t i = 1; i < clusters.size(); i++)
	{
		diffs[i - 1] = prev.getCentroid()[0] - clusters[i].getCentroid()[0];
		prev = clusters[i];
	}
	LOGDEB("Intercluster distances:");
	LOGDEB(diffs);

	size_t pos = 0;
	double max = 0;
	for (size_t i = 0; i < diffs.size(); i++)
	{
		if (diffs[i] > max)
		{
			max = diffs[i];
			pos = i;
		}
	}
	LOGDEB("{{max: {:g}, pos: {}}}"_format(max, pos));

	uint64_t &m1 = masks[masks.size() - pos - 1];
	uint64_t &m2 = masks[masks.size() - pos - 2];

	if (m1 <= m2)
	{
		LOGDEB("No mask changes");
		return;
	}

	m1 = (m1 | (m1 << 1)) & 0xfffff; // Add one bit to the left
	m2 = __builtin_popcount(m2) > 2 ? m2 & (m2 >> 1) : m2; // Remove one bit from the left, but keep a minimum of two

	set_masks(masks);

	LOGDEB("Classes Of Service:");
	for (size_t cos = masks.size() - 1; cos < masks.size(); cos--)
		LOGDEB(fmt::format("{{COS{}: {{mask: {:#x}}}}}", cos, masks[cos]));
}
