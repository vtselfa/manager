#pragma once

#include <functional>
#include <vector>
#include <cassert>

#include "cat-intel.hpp"
#include "kmeans.hpp"
#include "task.hpp"

namespace cat
{


const uint32_t min_num_ways = 2;
const uint32_t max_num_ways = 20;
const uint32_t complete_mask = ~(-1U << max_num_ways);
const uint32_t num_cos = 4;


namespace policy
{


// Base class that does nothing
class Base
{
	protected:

	CAT cat;

	public:

	Base() = default;

	void set_cat(CAT cat)      { this->cat = cat; }
	CAT& get_cat()             { return cat; }
	const CAT& get_cat() const { return cat; }

	virtual ~Base() = default;

	// Derived classes should perform their operations here.
	// The base class does nothing by default.
	virtual void apply(uint64_t, const std::vector<Task> &) {}
};


// Sort applications by slowdown and assign the slowest to COS3, the second most slowest
// to COS2, the third to COS1 and the rest to COS0.
class Slowfirst: public Base
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

	Slowfirst(uint64_t every, const std::vector<uint64_t> &masks) : Base(), every(every), masks(masks)
	{
		check_masks(masks);
	}

	virtual ~Slowfirst() = default;

	// It's important to NOT make distinctions between completed and not completed tasks...
	// We asume that the event we care about has been programed as ev2.
	virtual void apply(uint64_t current_interval, const std::vector<Task> &tasklist);
};
typedef Slowfirst Sf;


// Cluster appplications by slowdown and then map each cluster to a COS.
// Since there is a one-to-one mapping from clusters to COSs, no more than 4 clusters are allowed.
class SlowfirstClustered: public Slowfirst
{
	protected:

	size_t num_clusters;
	std::vector<Cluster> clusters;

	public:

	SlowfirstClustered(uint64_t every, std::vector<uint64_t> masks, size_t num_clusters) :
			Slowfirst(every, masks), num_clusters(num_clusters) {}

	virtual ~SlowfirstClustered() = default;

	virtual void apply(uint64_t current_interval, const std::vector<Task> &tasklist);
};
typedef SlowfirstClustered SfC;


// Applications are clustered and mapped to COS, but the amount of ways assigned to each COS is determined
// by a simple model, which uses the slowdown per cluster as input.
class SlowfirstClusteredAdjusted: public SlowfirstClustered
{
	public:

	SlowfirstClusteredAdjusted(uint64_t every, std::vector<uint64_t> masks, size_t num_clusters) :
			SlowfirstClustered(every, masks, num_clusters) {}

	virtual ~SlowfirstClusteredAdjusted() = default;

	virtual void apply(uint64_t current_interval, const std::vector<Task> &tasklist);
};
typedef SlowfirstClusteredAdjusted SfCA;


// Applications are clustered with the optimal number of clusters and mapped to COS, and the amount of ways assigned to each COS is determined
// by a simple model, which uses the slowdown per cluster as input.
class SlowfirstClusteredOptimallyAdjusted: public SlowfirstClustered
{
	public:

	class Model
	{
		std::unordered_map <std::string, std::function<double(double)>> models;
		std::function<double(double)> model;

		public:

		const std::string name;

		Model(const std::string &name);

		double operator() (double x) const { return model(x); }
	};

	Model model;

	// If num_clusters is not 0, then this number of clusters is used, instead of trying to find the optimal one
	SlowfirstClusteredOptimallyAdjusted(uint64_t every, uint32_t num_clusters, std::string model_str) :
			SlowfirstClustered(every, std::vector<uint64_t>(cat::num_cos, cat::complete_mask), num_clusters), model(model_str) {}

	virtual ~SlowfirstClusteredOptimallyAdjusted() = default;

	virtual void apply(uint64_t current_interval, const std::vector<Task> &tasklist);
};
typedef SlowfirstClusteredOptimallyAdjusted SfCOA;


}} // cat::policy
