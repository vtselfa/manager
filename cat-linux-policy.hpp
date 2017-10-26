#pragma once

#include "cat-policy.hpp"
#include "cat-linux.hpp"


namespace cat
{
namespace policy
{


class ClusteringBase
{
	public:

	ClusteringBase() = default;
	virtual ~ClusteringBase() = default;

	virtual clusters_t apply(const tasklist_t &tasklist);
};
typedef std::shared_ptr<ClusteringBase> ClusteringBase_ptr_t;


class Cluster_SF: public ClusteringBase
{
	public:

	int m;
	std::vector<int> sizes;

	Cluster_SF(const std::vector<int> &sizes) : sizes(sizes) {}
	virtual ~Cluster_SF() = default;

	virtual clusters_t apply(const tasklist_t &tasklist) override;
};


class Cluster_KMeans: public ClusteringBase
{
	public:

	int num_clusters;
	int max_clusters;
	EvalClusters eval_clusters;
	std::string event;
	bool sort_ascending;

	Cluster_KMeans(int num_clusters, int max_clusters, EvalClusters eval_clusters, std::string event, bool sort_ascending) :
			num_clusters(num_clusters), max_clusters(max_clusters),
			eval_clusters(eval_clusters), event(event),
			sort_ascending(sort_ascending) {}
	virtual ~Cluster_KMeans() = default;

	virtual clusters_t apply(const tasklist_t &tasklist) override;
};


typedef std::vector<uint32_t> ways_t;
class DistributingBase
{
	public:

	int min_ways;
	int max_ways;

	DistributingBase()
	{
		auto info = cat_read_info();
		min_ways = info["L3"].min_cbm_bits;
		max_ways = __builtin_popcount(info["L3"].cbm_mask);
	}

	DistributingBase(int min_ways, int max_ways) : min_ways(min_ways), max_ways(max_ways) {}

	uint32_t cut_mask(uint32_t mask)
	{
		return mask & ~((uint32_t) -1 << max_ways);
	}

	virtual ~DistributingBase() = default;

	virtual ways_t apply(const tasklist_t &, const clusters_t &) { return ways_t(); };
};
typedef std::shared_ptr<DistributingBase> DistributingBase_ptr_t;


class Distribute_N: public DistributingBase
{
	int n;

	public:

	Distribute_N(int n): n(n) {}
	Distribute_N(int min_ways, int max_ways, int n): DistributingBase(min_ways, max_ways), n(n) {}
	virtual ~Distribute_N() = default;

	virtual ways_t apply(const tasklist_t &, const clusters_t &clusters) override;
};


class Distribute_Static: public DistributingBase
{
	ways_t masks;

	public:

	Distribute_Static(const ways_t &masks) : masks(masks){}
	virtual ~Distribute_Static() = default;

	virtual ways_t apply(const tasklist_t &, const clusters_t &) override { return masks; }
};


class Distribute_RelFunc: public DistributingBase
{
	bool invert_metric;

	public:

	Distribute_RelFunc() = default;
	Distribute_RelFunc(bool invert_metric) : invert_metric(invert_metric) {};
	Distribute_RelFunc(int min_ways, int max_ways, bool invert_metric) :
			DistributingBase(min_ways, max_ways), invert_metric(invert_metric) {};
	virtual ~Distribute_RelFunc() = default;

	virtual ways_t apply(const tasklist_t &, const clusters_t &clusters) override;
};


class ClusterAndDistribute: public Base
{

	uint32_t every;
	ClusteringBase_ptr_t clustering;
	DistributingBase_ptr_t distributing;

	public:

	ClusterAndDistribute() = delete;
	ClusterAndDistribute(uint32_t every, ClusteringBase_ptr_t clustering, DistributingBase_ptr_t distributing) :
			every(every), clustering(clustering), distributing(distributing) {}


	std::shared_ptr<CATLinux> get_cat()
	{
		auto ptr =  std::dynamic_pointer_cast<CATLinux>(cat);
		if (ptr)
			return ptr;
		else
			throw_with_trace(std::runtime_error("Linux CAT implementation required"));
	}


	void tasks_to_closes(const tasklist_t &tasklist, const clusters_t &clusters)
	{
		assert(cat->get_max_closids() >= clusters.size());

		for(size_t clos = 0; clos < clusters.size(); clos++)
		{
			for (const auto point : clusters[clos].getPoints())
			{
				const auto &task = *std::find_if(tasklist.begin(), tasklist.end(), [&point](const auto &task){return point->id == task->id;});
				get_cat()->add_task(clos, task->pid);
			}
		}
	}


	void ways_to_closes(const ways_t &ways)
	{
		assert(cat->get_max_closids() >= ways.size());
		for(size_t clos = 0; clos < ways.size(); clos++)
			get_cat()->set_cbm(clos, ways[clos]);

	}


	virtual void apply(uint64_t current_interval, const tasklist_t &tasklist)
	{
		// Apply the policy only when the amount of intervals specified has passed
		if (current_interval % every != 0)
			return;
		auto clusters = clustering->apply(tasklist);
		auto ways = distributing->apply(tasklist, clusters);
		show(tasklist, clusters, ways);
		tasks_to_closes(tasklist, clusters);
		ways_to_closes(ways);
	}

	void show(const tasklist_t &tasklist, const clusters_t &clusters, const ways_t &ways);
};

}} // cat::policy
