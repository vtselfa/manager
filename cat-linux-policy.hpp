#pragma once

#include "cat-policy.hpp"
#include "cat-linux.hpp"


namespace cat
{
namespace policy
{


//-----------------------------------------------------------------------------
// Aux functions
//-----------------------------------------------------------------------------


void tasks_to_closes(catlinux_ptr_t cat, const tasklist_t &tasklist, const clusters_t &clusters);
std::string cluster_to_tasks(const Cluster &cluster, const tasklist_t &tasklist);


class ClusteringBase
{
	public:

	class CouldNotCluster : public std::runtime_error
	{
		public:
		CouldNotCluster(const std::string &msg) : std::runtime_error(msg) {};
	};

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

	Cluster_SF(const std::vector<int> &_sizes) : sizes(_sizes) {}
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

	Cluster_KMeans(int _num_clusters, int _max_clusters, EvalClusters _eval_clusters, std::string _event, bool _sort_ascending) :
			num_clusters(_num_clusters), max_clusters(_max_clusters),
			eval_clusters(_eval_clusters), event(_event),
			sort_ascending(_sort_ascending) {}
	virtual ~Cluster_KMeans() = default;

	virtual clusters_t apply(const tasklist_t &tasklist) override;
};


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

	DistributingBase(int _min_ways, int _max_ways) : min_ways(_min_ways), max_ways(_max_ways) {}

	uint32_t cut_mask(uint32_t mask)
	{
		return mask & ~((uint32_t) -1 << max_ways);
	}

	virtual ~DistributingBase() = default;

	virtual cbms_t apply(const tasklist_t &, const clusters_t &) { return cbms_t(); };
};
typedef std::shared_ptr<DistributingBase> DistributingBase_ptr_t;


class Distribute_N: public DistributingBase
{
	int n;

	public:

	Distribute_N(int _n): n(_n) {}
	Distribute_N(int _min_ways, int _max_ways, int _n): DistributingBase(_min_ways, _max_ways), n(_n) {}
	virtual ~Distribute_N() = default;

	virtual cbms_t apply(const tasklist_t &, const clusters_t &clusters) override;
};


class Distribute_Static: public DistributingBase
{
	cbms_t masks;

	public:

	Distribute_Static(const cbms_t &_masks) : masks(_masks){}
	virtual ~Distribute_Static() = default;

	virtual cbms_t apply(const tasklist_t &, const clusters_t &) override { return masks; }
};


class Distribute_RelFunc: public DistributingBase
{
	bool invert_metric;

	public:

	Distribute_RelFunc() = default;
	Distribute_RelFunc(bool _invert_metric) : invert_metric(_invert_metric) {};
	Distribute_RelFunc(int _min_ways, int _max_ways, bool _invert_metric) :
			DistributingBase(_min_ways, _max_ways), invert_metric(_invert_metric) {};
	virtual ~Distribute_RelFunc() = default;

	virtual cbms_t apply(const tasklist_t &, const clusters_t &clusters) override;
};


//-----------------------------------------------------------------------------
// Linux CAT Policies
//-----------------------------------------------------------------------------


class LinuxBase : public Base
{
	public:

	LinuxBase() = default;
	virtual ~LinuxBase() = default;

	// Safely cast CAT to LinuxCat
	std::shared_ptr<CATLinux> get_cat();

	// Derived classes should perform their operations here. This base class does nothing by default.
	virtual void apply(uint64_t, const tasklist_t &) {}
};


class SquareWave: public LinuxBase
{
	bool is_down;

	public:

	class Wave
	{
		public:
		bool is_down;
		uint32_t interval;
		cbm_t up;
		cbm_t down;
		Wave() = default;
		Wave(uint32_t _interval, cbm_t _up, cbm_t _down) : is_down(false), interval(_interval), up(_up), down(_down) {}
	};

	std::vector<Wave> waves;
	ClusteringBase clustering;

	SquareWave() = delete;
	SquareWave(std::vector<Wave> _waves) : waves(_waves), clustering() {}

	virtual void apply(uint64_t current_interval, const tasklist_t &tasklist);
};


class ClusterAndDistribute: public LinuxBase
{

	uint32_t every;
	ClusteringBase_ptr_t clustering;
	DistributingBase_ptr_t distributing;

	public:

	ClusterAndDistribute() = delete;
	ClusterAndDistribute(uint32_t _every, ClusteringBase_ptr_t _clustering, DistributingBase_ptr_t _distributing) :
			every(_every), clustering(_clustering), distributing(_distributing) {}

	virtual void apply(uint64_t current_interval, const tasklist_t &tasklist);
	void show(const tasklist_t &tasklist, const clusters_t &clusters, const cbms_t &ways);
};

}} // cat::policy
