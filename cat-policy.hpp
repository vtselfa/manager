#pragma once

#include <cassert>
#include <functional>
#include <unordered_map>
#include <vector>

#include "cat-intel.hpp"
#include "kmeans.hpp"
#include "task.hpp"

namespace cat
{


const uint32_t min_num_ways = 2;
const uint32_t max_num_ways = 20;
const uint32_t complete_mask = ~(-1U << max_num_ways);
typedef std::tuple<uint32_t, uint64_t> pair;

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


// Gathers sames measurements as HitsGain
// but makes no changes to CAT configuration
class NoPart: public Base
{
	protected:

	uint64_t every = -1;
	std::string stats = "total";

	public:

	virtual ~NoPart() = default;

	NoPart(uint64_t every, std::string stats) : Base(), every(every), stats(stats)
	{}

	virtual void apply(uint64_t, const std::vector<Task> &);
};
typedef NoPart NP;

// Sorts applications by number of hits in L3 and recompensates those with
// more hits, i.e., those who have made best use of its ways and
// penalizes those who have less number of hits
class HitsGain: public Base
{
	protected:

	uint64_t every = -1;
	int ways_increment = 1;
	std::string stats = "total";
	//flag to control if changes made in HitsGain policy
	bool maskModified = false;
	//variable to store IPC to use in next interval
	double ipcTotalPrev;
	//vector to store previous list of cores, ways
	std::vector<pair> vPrev;
	//array indexed by core num -> true if core has l3_occup = 0
	bool noChange [8] = {false, false, false, false, false, false, false, false};

	public:

	virtual ~HitsGain() = default;

	HitsGain(uint64_t every, int ways_increment, std::string stats) : Base(), every(every), ways_increment(ways_increment), stats(stats){}

	//configure CAT
	void undo_changes(const std::vector<pair> &v);
	void add_way(uint32_t core);
	void subtract_way(uint32_t core);

	// metodo apply
	virtual void apply(uint64_t, const std::vector<Task> &);
};
typedef HitsGain HG;



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

	const Model model;
	const EvalClusters eval_clusters;
	const bool alternate_sides;
	const double min_stall_ratio;
	const bool detect_outliers;
	const std::vector<uint32_t> cluster_sizes; // Fixed cluster sizes
	const bool min_max;

	// If num_clusters is not 0, then this number of clusters is used, instead of trying to find the optimal one
	SlowfirstClusteredOptimallyAdjusted(uint64_t every, uint32_t num_clusters, const std::string &model_str, bool alternate_sides,
			double min_stall_ratio, bool detect_outliers, const std::string &eval_clusters_str, const std::vector<uint32_t> &cluster_sizes, bool min_max)
		:
			SlowfirstClustered(every, {}, num_clusters),
			model(model_str),
			eval_clusters(str_to_evalclusters(eval_clusters_str)),
			alternate_sides(alternate_sides),
			min_stall_ratio(min_stall_ratio),
			detect_outliers(detect_outliers),
			cluster_sizes(cluster_sizes),
			min_max(min_max)
	{}

	virtual ~SlowfirstClusteredOptimallyAdjusted() = default;

	virtual void apply(uint64_t current_interval, const std::vector<Task> &tasklist);

	EvalClusters str_to_evalclusters(const std::string &str)
	{
		if (str == "dunn")
			return EvalClusters::dunn;
		if (str == "silhouette")
			return EvalClusters::silhouette;
		throw_with_trace(std::runtime_error("Unknown eval_clusters algorithm"));
	}
};
typedef SlowfirstClusteredOptimallyAdjusted SfCOA;


}} // cat::policy
