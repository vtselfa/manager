#include <algorithm>
#include <iostream>
#include <tuple>

#include "cat-policy.hpp"
#include "kmeans.hpp"


void CAT_Policy_Slowfirst::set_masks(const std::vector<uint64_t> &masks)
{
	this->masks = masks;
	for (uint32_t i = 0; i < masks.size(); i++)
		cat.set_cos_mask(i, masks[i]);
}


void CAT_Policy_Slowfirst::check_masks(const std::vector<uint64_t> &masks) const
{
	uint64_t last_mask = 0;
	for (auto mask : masks)
	{
		if (last_mask > mask)
		{
			std::cerr << "The masks for the slowfirst CAT policy may be in the wrong order" << std::endl;
			break;
		}
		last_mask = mask;
	}
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
	set_masks(masks);

	uint64_t last_mask = 0;
	for (auto mask : masks)
	{
		if (last_mask > mask)
		{
			std::cerr << "The masks for the slowfirst CAT policy may be in the wrong order" << std::endl;
			break;
		}
		last_mask = mask;
	}

	for (uint32_t t = 0; t < tasklist.size(); t++)
	{
		const Task &task = tasklist[t];
		uint64_t l2_miss_stalls = task.stats_acumulated.event[2];
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
	auto clusters = std::vector<Cluster>();

	cat.reset();
	set_masks(masks);

	// Put data in the format KMeans expects
	for (uint32_t t = 0; t < tasklist.size(); t++)
	{
		const Task &task = tasklist[t];
		uint64_t l2_miss_stalls = task.stats_acumulated.event[2];
		data.push_back(Point(t, {(double) l2_miss_stalls}));
	}

	KMeans::clusterize(num_clusters, data, clusters, 100);

	// Sort clusters in descending order
	std::sort(begin(clusters), end(clusters),
			[](const Cluster &c1, const Cluster &c2)
			{
				return c1.getCentroid()[0] > c2.getCentroid()[0];
			});

	// Iterate clusters
	for (size_t c = 0; c < clusters.size(); c++)
	{
		size_t cos  = masks.size() - c - 1;
		const auto &cluster = clusters[c];

		// Iterate tasks in cluster and put them in the adequate COS
		for(const auto &item : cluster.getPoints())
		{
			const size_t index = item.first;
			const Task &task = tasklist[index];
			cat.set_cos_cpu(cos, task.cpu);
		}
	}
}
