#include <algorithm>
#include <iostream>
#include <tuple>

#include "cat-policy.hpp"


void CAT_Policy_Slowfirst::set_masks(std::vector<uint64_t> &masks)
{
	masks = masks;
	for (uint32_t i = 0; i < masks.size(); i++)
		cat.set_cos_mask(i, masks[i]);
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
