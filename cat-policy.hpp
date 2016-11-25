#pragma once

#include <vector>

#include "cat-intel.hpp"
#include "kmeans.hpp"
#include "task.hpp"


class CAT_Policy
{
	protected:

	const uint32_t min_num_ways = 2;
	const uint32_t complete_mask = 0xfffff;
	const uint32_t num_ways = 20;
	const uint32_t num_cos = 4;
	CAT cat;

	public:

	CAT_Policy() = default;

	void set_cat(CAT cat)      { this->cat = cat; }
	CAT& get_cat()             { return cat; }
	const CAT& get_cat() const { return cat; }

	virtual ~CAT_Policy() = default;

	// Derived classes should perform their operations here.
	// The base class does nothing by default.
	virtual void adjust(uint64_t, const std::vector<Task> &) {}
};


class CAT_Policy_Slowfirst: public CAT_Policy
{
	protected:

	// This policy will be applied every this number of intervals
	uint64_t every = -1;

	// Masks should be in reversal order of importance i.e. first mask is gonna be used by COS0,
	// wich will contain most of the processes. The greater the COS number the more advantageous
	// is presumed to be. At least that is what the algorithm expects.
	std::vector<uint64_t> masks;

	// Configure CAT with the provided masks
	void set_masks(const std::vector<uint64_t> &masks);
	void check_masks(const std::vector<uint64_t> &masks) const;

	public:

	CAT_Policy_Slowfirst(uint64_t every, const std::vector<uint64_t> &masks) : CAT_Policy(), every(every), masks(masks)
	{
		check_masks(masks);
	}

	virtual ~CAT_Policy_Slowfirst() = default;

	// It's important to NOT make distinctions between completed and not completed tasks...
	// We asume that the event we care about has been programed as ev2.
	virtual void adjust(uint64_t current_interval, const std::vector<Task> &tasklist);
};

class CAT_Policy_SF_Kmeans: public CAT_Policy_Slowfirst
{
	protected:

	size_t num_clusters;
	std::vector<Cluster> clusters;

	public:

	CAT_Policy_SF_Kmeans(uint64_t every, std::vector<uint64_t> masks, size_t num_clusters) :
			CAT_Policy_Slowfirst(every, masks), num_clusters(num_clusters) {}

	virtual ~CAT_Policy_SF_Kmeans() = default;

	virtual void adjust(uint64_t current_interval, const std::vector<Task> &tasklist);
};


class CAT_Policy_SF_Kmeans2: public CAT_Policy_SF_Kmeans
{
	protected:

	uint64_t every_m;

	public:

	CAT_Policy_SF_Kmeans2(uint64_t every, std::vector<uint64_t> masks, size_t num_clusters, uint64_t every_m) :
			CAT_Policy_SF_Kmeans(every, masks, num_clusters), every_m(every_m) {}

	virtual ~CAT_Policy_SF_Kmeans2() = default;

	virtual void adjust(uint64_t current_interval, const std::vector<Task> &tasklist);
};
