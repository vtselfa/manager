#pragma once

#include "cat-policy.hpp"
#include "cat-linux.hpp"


#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/max.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/rolling_window.hpp>
#include <boost/accumulators/statistics/rolling_variance.hpp>


namespace cat
{


namespace policy
{

namespace acc = boost::accumulators;

// No partition policy
class NoPart: public Base
{
    protected:
	std::shared_ptr<CAT> catLinux = std::make_shared<CATLinux>();
    uint64_t every = -1;
    std::string stats = "total";

	double expected_IPC = 0;

    public:
    virtual ~NoPart() = default;
    NoPart(uint64_t _every, std::string _stats) : every(_every), stats(_stats){}
    virtual void apply(uint64_t, const tasklist_t &) override;
};
typedef NoPart NP;


class CriticalAlone: public Base
{

    protected:
    uint64_t every = -1;
    uint64_t firstInterval = 1;

    //Masks of CLOS
    uint64_t maskCrCLOS = 0xfffff;
    uint64_t num_ways_CLOS_2 = 20;
    uint64_t maskNonCrCLOS = 0xfffff;
    uint64_t num_ways_CLOS_1 = 20;

    uint64_t num_shared_ways = 0;

    //Control of the changes made in the masks
    uint64_t state = 0;
    double expectedIPCtotal = 0;
    double ipc_CR_prev = 0;
    double ipc_NCR_prev = 0;

    double mpkiL3Mean = 0;
	double stdmpkiL3Mean = 0;

    bool firstTime = true;

    uint64_t IDLE_INTERVALS = 5;
    uint64_t idle_count = IDLE_INTERVALS;
    bool idle = false;

	// Define accumulators
	typedef acc::accumulator_set<
		double, acc::stats<
			acc::tag::rolling_mean,
			acc::tag::rolling_variance
		>
	>
	ca_accum_t;

	ca_accum_t macc;

    //vector to store if task is assigned to critical CLOS
	typedef std::tuple<pid_t, uint64_t> pair_t;
    std::vector<pair_t> taskIsInCRCLOS;
	std::vector<pair_t> pid_CPU;

	// number of times a task has been critical
	std::map<pid_t,uint64_t> frequencyCritical;

    public:

	std::shared_ptr<CATLinux> get_cat()
    {
		auto ptr = std::dynamic_pointer_cast<CATLinux>(cat);
        if (ptr)
        	return ptr;
    	else
        	throw_with_trace(std::runtime_error("Linux CAT implementation required"));
    }

	//typedef std::tuple<pid_t, uint64_t> pair_t

    CriticalAlone(uint64_t _every, uint64_t _firstInterval) : every(_every), firstInterval(_firstInterval), macc(acc::tag::rolling_window::window_size = 10u) {}

    virtual ~CriticalAlone() = default;

    //configure CAT
    void reset_configuration(const tasklist_t &);

	// calculate median of vector of tuples
	typedef std::tuple<pid_t, double> pairD_t;
	double medianV(std::vector<pairD_t> &vec);


    // metodo apply
    virtual void apply(uint64_t, const tasklist_t &) override;
};
typedef CriticalAlone CA;



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

	DistributingBase(int _min_ways, int _max_ways) : min_ways(_min_ways), max_ways(_max_ways) {}

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

	Distribute_N(int _n): n(_n) {}
	Distribute_N(int _min_ways, int _max_ways, int _n): DistributingBase(_min_ways, _max_ways), n(_n) {}
	virtual ~Distribute_N() = default;

	virtual ways_t apply(const tasklist_t &, const clusters_t &clusters) override;
};


class Distribute_Static: public DistributingBase
{
	ways_t masks;

	public:

	Distribute_Static(const ways_t &_masks) : masks(_masks){}
	virtual ~Distribute_Static() = default;

	virtual ways_t apply(const tasklist_t &, const clusters_t &) override { return masks; }
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

	virtual ways_t apply(const tasklist_t &, const clusters_t &clusters) override;
};


class ClusterAndDistribute: public Base
{

	uint32_t every;
	ClusteringBase_ptr_t clustering;
	DistributingBase_ptr_t distributing;

	public:

	ClusterAndDistribute() = delete;
	ClusterAndDistribute(uint32_t _every, ClusteringBase_ptr_t _clustering, DistributingBase_ptr_t _distributing) :
			every(_every), clustering(_clustering), distributing(_distributing) {}


	std::shared_ptr<CATLinux> get_cat()
	{
		auto ptr = std::dynamic_pointer_cast<CATLinux>(cat);
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
				const auto &task = tasks_find(tasklist, point->id);
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

	virtual void apply(uint64_t current_interval, const tasklist_t &tasklist);

	void show(const tasklist_t &tasklist, const clusters_t &clusters, const ways_t &ways);
};

}} // cat::policy
