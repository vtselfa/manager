#pragma once

#include "cat-policy.hpp"
#include "cat-linux.hpp"


namespace cat
{
namespace policy
{


// Sort applications by slowdown and assign the slowest to COS3, the second most slowest
// to COS2, the third to COS1 and the rest to COS0.
class SFL: public Base
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

	std::shared_ptr<CATLinux> get_cat(){ return std::dynamic_pointer_cast<CATLinux>(cat); }

	public:

	SFL(uint64_t every, const std::vector<uint64_t> &masks) : Base(), every(every), masks(masks)
	{
		check_masks(masks);
	}

	virtual ~SFL() = default;

	virtual void apply(uint64_t current_interval, const std::vector<Task> &tasklist);
};


class ClusteringBase
{
	public:

	ClusteringBase() = default;
	virtual ~ClusteringBase() = default;

	virtual clusters_t apply(const std::vector<Task> &tasklist) = 0;
};
typedef std::shared_ptr<ClusteringBase> ClusteringBase_ptr_t;


class Cluster_SF: public ClusteringBase
{
	public:

	int m;
	std::vector<int> sizes;

	Cluster_SF(const std::vector<int> &sizes) : sizes(sizes) {}
	virtual ~Cluster_SF() = default;

	clusters_t apply(const std::vector<Task> &tasklist) override;
};


class Cluster_KMeans: public ClusteringBase
{
	public:

	int num_clusters;
	int max_clusters;
	EvalClusters eval_clusters;

	Cluster_KMeans(int num_clusters, int max_clusters, EvalClusters eval_clusters) : num_clusters(num_clusters), max_clusters(max_clusters), eval_clusters(eval_clusters) {}
	virtual ~Cluster_KMeans() = default;

	clusters_t apply(const std::vector<Task> &tasklist) override;
};


typedef std::vector<uint32_t> ways_t;
class DistributingBase
{
	public:

	int min_ways;
	int max_ways;

	DistributingBase() = delete;
	DistributingBase(int min_ways, int max_ways) : min_ways(min_ways), max_ways(max_ways) {}

	uint32_t cut_mask(uint32_t mask)
	{
		return mask & ~((uint32_t) -1 << max_ways);
	}

	virtual ~DistributingBase() = default;

	virtual ways_t apply(const std::vector<Task> &tasklist, clusters_t &clusters) = 0;
};
typedef std::shared_ptr<DistributingBase> DistributingBase_ptr_t;


class Distribute_N: public DistributingBase
{
	int n;

	Distribute_N(int min_ways, int max_ways, int n): DistributingBase(min_ways, max_ways), n(n) {}
	virtual ~Distribute_N() = default;

	ways_t apply(const std::vector<Task> &, clusters_t &clusters) override;
};


class Distribute_RelFunc: public DistributingBase
{

	Distribute_RelFunc(int min_ways, int max_ways): DistributingBase(min_ways, max_ways) {}
	virtual ~Distribute_RelFunc() = default;

	ways_t apply(const std::vector<Task> &, clusters_t &clusters) override;
};





class ClusterAndDistribute: public Base
{
	public:

	uint32_t every;
	ClusteringBase_ptr_t clustering;
	DistributingBase_ptr_t distributing;

	ClusterAndDistribute() = delete;
	ClusterAndDistribute(uint32_t every, ClusteringBase_ptr_t clustering, DistributingBase_ptr_t distributing) :
			every(every), clustering(clustering), distributing(distributing) {}

	std::shared_ptr<CATLinux> get_cat(){ return std::dynamic_pointer_cast<CATLinux>(cat); }


	void tasks_to_closes(const tasklist_t &tasklist, const clusters_t &clusters)
	{
		assert(cat->get_max_closids() <= clusters.size());

		for(size_t clos = 0; clos < clusters.size(); clos++)
		{
			for (const auto point : clusters[clos].getPoints())
			{
				get_cat()->add_task(clos, tasklist[point->id].pid);
			}
		}
	}


	void ways_to_closes(const ways_t &ways)
	{
		assert(cat->get_max_closids() <= ways.size());
		for(size_t clos = 0; clos < ways.size(); clos++)
			get_cat()->set_cbm(clos, ways[clos]);

	}


	virtual void apply(uint64_t current_interval, const std::vector<Task> &tasklist)
	{
		auto clusters = clustering->apply(tasklist);
		auto ways = distributing->apply(tasklist, clusters);
		tasks_to_closes(tasklist, clusters);
		ways_to_closes(ways);
	}
};

}} // cat::policy
