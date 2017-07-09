#include <cmath>
#include <iostream>
#include <iterator>
#include <tuple>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/max.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/variance.hpp>
#include <cxx-prettyprint/prettyprint.hpp>
#include <fmt/format.h>

#include "cat-policy.hpp"
#include "log.hpp"


#define L3_HITS "MEM_LOAD_UOPS_RETIRED.L3_HIT"
#define L3_MISSES "MEM_LOAD_UOPS_RETIRED.L3_MISS"
#define STALLS_TOTAL "CYCLE_ACTIVITY.STALLS_TOTAL"


namespace cat
{
namespace policy
{


namespace acc = boost::accumulators;
using fmt::literals::operator""_format;
using std::string;


void NoPart::apply(uint64_t current_interval, const std::vector<Task> &tasklist)
{
	// Apply only when the amount of intervals specified has passed
	if (current_interval % every != 0)
		return;

	double ipcTotal = 0;

	LOGINF("CAT Policy name: NoPart");
	LOGINF("Using stats {}"_format(stats));

	// GATHER DATA
	for (uint32_t t = 0; t < tasklist.size(); t++)
	{
		const Task &task = tasklist[t];
		std::string taskName = task.executable;
		LOGINF("Task name = {}"_format(taskName));

		uint64_t l3_hits, cycles, l3_occup;
		double ipc, hits_per_occup;

		assert((stats == "total") | (stats == "interval"));

		if (stats == "total")
		{
			// L3 hits
			l3_hits = acc::sum(task.stats_total.events.at(L3_HITS));

			// Cycles and IPC
			cycles = task.stats_total.cycles;
			ipc = task.stats_total.ipc;

			// L3 occupation
			l3_occup = task.stats_total.l3_kbytes_occ;
		}
		else if (stats == "interval")
		{
			// L3 hits
			l3_hits = acc::sum(task.stats_interval.events.at(L3_HITS));

			// Cycles and IPC
			cycles = task.stats_interval.cycles;
			ipc = task.stats_interval.ipc;

			// L3 occupation
			l3_occup = task.stats_interval.l3_kbytes_occ;
		}


		ipcTotal += ipc;

		if (l3_occup == 0)
		{
			LOGERR("l3_occup = 0 -> hits_per_occup = 0");
			hits_per_occup = 0;
		}
		else
		{
			hits_per_occup = l3_hits / l3_occup;
		}

		LOGINF("Task {} (COS {}) has {} L3 hits, {} KB L3 occup, {} L3 hits/occup"_format(
				taskName, cat.get_cos_of_cpu(task.cpu), l3_hits, l3_occup, hits_per_occup));
	}
	LOGINF("IPC total of interval {} = {:.2f}"_format(current_interval, ipcTotal));
}


void HitsGain::subtract_way(uint32_t core)
{
	uint64_t newMask;
	size_t cos = cat.get_cos_of_cpu(core);
	uint64_t mask = cat.get_cos_mask(cos);
	//LOGINF("S_W: Cos {} before has mask {:#x} "_format(cos,mask));

	// shift 1 bit to the left the current mask
	newMask = ((mask << ways_increment) & mask) | 0xC0000;
	//LOGINF("S_W: Cos {} after has mask {:#x} "_format(cos,newMask));
	cat.set_cos_mask(cos, newMask);

	LOGINF("COS number {} has mask {:#x} after subtracting one way "_format(cos, cat.get_cos_mask(cos)));
}


void HitsGain::add_way(uint32_t core)
{
	uint64_t newMask;
	size_t cos = cat.get_cos_of_cpu(core);
	uint64_t mask = cat.get_cos_mask(cos);

	// shift 1 bit to the right the current mask OR current mask
	newMask = (mask >> ways_increment) | mask;
	cat.set_cos_mask(cos, newMask);

	LOGINF("COS number {} has mask {:#x} after adding one way "_format(cos, cat.get_cos_mask(cos)));
}


void HitsGain::undo_changes(const std::vector<pair> &v)
{
	for (const auto &item : v)
	{
		uint32_t core = std::get<0>(item);
		uint64_t mask = std::get<1>(item);
		size_t cos = cat.get_cos_of_cpu(core);
		cat.set_cos_mask(cos, mask);
		LOGINF("COS number {} has mask {:#x} after undoing changes "_format(cos, mask));
	}

	LOGINF("Changes from previous interval undone");
}


void HitsGain::apply(uint64_t current_interval, const std::vector<Task> &tasklist)
{

	// Apply only when the amount of intervals specified has passed
	if (current_interval % every != 0)
		return;

	// (Core, hits) tuple
	typedef std::tuple<uint32_t, uint64_t> pair;
	auto v = std::vector<pair>();

	double ipcTotal = 0;

	LOGINF("CAT Policy name: HitsGain");
	LOGINF("Using stats {}"_format(stats));

	// GATHER DATA
	for (uint32_t t = 0; t < tasklist.size(); t++)
	{
		const Task &task = tasklist[t];
		std::string taskName = task.executable;

		uint64_t l3_hits, cycles, l3_occup;
		double ipc, hits_per_occup;

		assert((stats == "total") | (stats == "interval"));

		if (stats == "total")
		{
			// L3 hits
			l3_hits = acc::sum(task.stats_total.events.at(L3_HITS));

			// Cycles and IPC
			cycles = task.stats_total.cycles;
			ipc = task.stats_total.ipc;

			// L3 occupation
			l3_occup = task.stats_total.l3_kbytes_occ;
		}
		else if (stats == "interval")
		{
			// L3 hits
			l3_hits = acc::sum(task.stats_interval.events.at(L3_HITS));

			// Cycles and IPC
			cycles = task.stats_interval.cycles;
			ipc = task.stats_interval.ipc;

			// L3 occupation
			l3_occup = task.stats_interval.l3_kbytes_occ;
		}

		ipcTotal += ipc;

		if (l3_occup == 0)
		{
			LOGERR("l3_occup = 0 -> hits_per_occup = 0 -> NO CHANGES WILL BE APPLIED");
			hits_per_occup = 0;
			// don't modify the mask of the CAT in this interval
			noChange[task.cpu] = true;
		}
		else
		{
			hits_per_occup = l3_hits / l3_occup;
		}

		// LOGINF("l3_hits {} / l3_occup {} = hits_per_occup {} "_format(l3_hits, l3_occup,
		// hits_per_occup));
		v.push_back(std::make_pair(task.cpu, hits_per_occup));


		LOGINF("Task {} (COS {}) has {} L3 hits, {} KB L3 occup, {} L3 hits/occup"_format(
				taskName, cat.get_cos_of_cpu(task.cpu), l3_hits, l3_occup, hits_per_occup));
	}

	LOGINF("IPC total of interval {} = {:.2f}"_format(current_interval, ipcTotal));

	// UNDO CHANGES ?
	// compare ipcTotal of this interval with previous interval with a 5% threshold
	// if worse && mask changed in previous interval --> undo changes
	LOGINF("ipcTotalPrev {}, (ipcTotalPrev*0.995) {}"_format(ipcTotalPrev, (ipcTotalPrev*0.995)));
	if (ipcTotal < (ipcTotalPrev*0.995) && maskModified)
	{
		LOGINF("Worst total IPC than previous interval -->");
		undo_changes(vPrev);
		maskModified = false;
	}
	else
	{
		// Save current CAT configuration in case of needing to undo
		// changes in the next interval
		vPrev.clear();
		for (auto const &item : v)
		{
			uint32_t core = std::get<0>(item);
			size_t cosToSave = cat.get_cos_of_cpu(core);
			uint64_t maskToSave = cat.get_cos_mask(cosToSave);
			vPrev.push_back(std::make_pair(core, maskToSave));
		}

		// PROCESS DATA
		// Sort in descending order by hits
		std::sort(begin(v), end(v),
				[](const pair &t1, const pair &t2) { return std::get<1>(t1) > std::get<1>(t2); });

		// APPLY MASK CHANGES
		// Allow only 8 apps
		uint32_t size_v = v.size();
		//LOGINF("Size of vector v = {}"_format(size_v));
		assert(size_v==8);

		// 3 apps with highest num of hits: add one way, if number of ways is lower than 20
		if(!noChange[std::get<0>(v[0])]){
			add_way(std::get<0>(v[0]));
		}else{
			size_t cosn = cat.get_cos_of_cpu(std::get<0>(v[0]));
			uint64_t maskn = cat.get_cos_mask(cosn);
			LOGINF("COS number {} has mask {:#x} after NO change "_format(cosn, maskn));
			noChange[std::get<0>(v[0])] = false;
		}

		if(!noChange[std::get<0>(v[1])]){
			add_way(std::get<0>(v[1]));
		}else{
			size_t cosn = cat.get_cos_of_cpu(std::get<0>(v[1]));
			uint64_t maskn = cat.get_cos_mask(cosn);
			LOGINF("COS number {} has mask {:#x} after NO change "_format(cosn, maskn));
			noChange[std::get<0>(v[1])] = false;
		}

		if(!noChange[std::get<0>(v[2])]){
			add_way(std::get<0>(v[2]));
		}else{
			size_t cosn = cat.get_cos_of_cpu(std::get<0>(v[2]));
			uint64_t maskn = cat.get_cos_mask(cosn);
			LOGINF("COS number {} has mask {:#x} after NO change "_format(cosn, maskn));
			noChange[std::get<0>(v[2])] = false;
		}


		// 3 apps with lowest num of hits: subtract one way, if number of ways is greater than 2
		if(!noChange[std::get<0>(v[7])]){
			subtract_way(std::get<0>(v[7]));
		}else{
			size_t cosn = cat.get_cos_of_cpu(std::get<0>(v[7]));
			uint64_t maskn = cat.get_cos_mask(cosn);
			LOGINF("COS number {} has mask {:#x} after NO change "_format(cosn, maskn));
			noChange[std::get<0>(v[7])] = false;
		}

		if(!noChange[std::get<0>(v[6])]){
			subtract_way(std::get<0>(v[6]));
		}else{
			size_t cosn = cat.get_cos_of_cpu(std::get<0>(v[6]));
			uint64_t maskn = cat.get_cos_mask(cosn);
			LOGINF("COS number {} has mask {:#x} after NO change "_format(cosn, maskn));
			noChange[std::get<0>(v[6])] = false;
		}

		if(!noChange[std::get<0>(v[5])]){
			subtract_way(std::get<0>(v[5]));
		}else{
			size_t cosn = cat.get_cos_of_cpu(std::get<0>(v[5]));
			uint64_t maskn = cat.get_cos_mask(cosn);
			LOGINF("COS number {} has mask {:#x} after NO change "_format(cosn, maskn));
			noChange[std::get<0>(v[5])] = false;
		}

		// other 2 apps are left unchanged
		size_t cos3 = cat.get_cos_of_cpu(std::get<0>(v[3]));
		uint64_t mask3 = cat.get_cos_mask(cos3);
		size_t cos4 = cat.get_cos_of_cpu(std::get<0>(v[4]));
		uint64_t mask4 = cat.get_cos_mask(cos4);

		LOGINF("COS number {} has mask {:#x} after NO change "_format(cos3, mask3));
		LOGINF("COS number {} has mask {:#x} after NO change "_format(cos4, mask4));

		//set noChange to false if it ws true
		if(noChange[std::get<0>(v[3])]){ noChange[std::get<0>(v[3])] = false; }
		if(noChange[std::get<0>(v[4])]){ noChange[std::get<0>(v[4])] = false; }

		// set modified flag to true and store total IPC
		maskModified = true;
		ipcTotalPrev = ipcTotal;
	}
}


void Slowfirst::set_masks(const std::vector<uint64_t> &masks)
{
	this->masks = masks;
	for (uint32_t i = 0; i < masks.size(); i++)
		cat.set_cos_mask(i, masks[i]);
}


void Slowfirst::check_masks(const std::vector<uint64_t> &masks) const
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


void Slowfirst::apply(uint64_t current_interval, const std::vector<Task> &tasklist)
{
	// It's important to NOT make distinctions between completed and not completed tasks...
	// We asume that the event we care about has been programed as ev2.

	// Apply only when the amount of intervals specified has passed
	if (current_interval % every != 0)
		return;

	LOGDEB("function: {}, interval: {}"_format(__PRETTY_FUNCTION__, current_interval));

	// (Core, Combined stalls) tuple
	typedef std::tuple<uint32_t, uint64_t> pair;
	auto v = std::vector<pair>();

	cat.reset();
	check_masks(masks);
	set_masks(masks);

	for (uint32_t t = 0; t < tasklist.size(); t++)
	{
		uint64_t l2_miss_stalls;
		const Task &task = tasklist[t];
		try
		{
			l2_miss_stalls = acc::sum(task.stats_total.events.at("CYCLE_ACTIVITY.STALLS_TOTAL"));
		}
		catch (const std::exception &e)
		{
			std::string msg = "This policy requires the event CYCLE_ACTIVITY.STALLS_TOTAL. The events monitorized are:";
			for (const auto &kv : task.stats_total.events)
				msg += "\n" + kv.first;
			throw_with_trace(std::runtime_error(msg));
		}
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


void SlowfirstClustered::apply(uint64_t current_interval, const std::vector<Task> &tasklist)
{
	// Apply the policy only when the amount of intervals specified has passed
	if (current_interval % every != 0)
		return;

	assert(num_clusters > 0);

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
		uint64_t l2_miss_stalls = acc::sum(task.stats_total.events.at("CYCLE_ACTIVITY.STALLS_TOTAL"));
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
		for(const auto &p : cluster.getPoints())
		{
			const Task &task = tasklist[p->id];
			cat.set_cos_cpu(cos, task.cpu);
			task_ids += i < cluster.getPoints().size() - 1 ?
					std::to_string(task.id) + ", ":
					std::to_string(task.id);
			i++;
		}

		LOGDEB(fmt::format("{{COS{}: {{mask: {:#x}, tasks: [{}]}}}}", cos, masks[cos], task_ids));
	}
}


void SlowfirstClusteredAdjusted::apply(uint64_t current_interval, const std::vector<Task> &tasklist)
{
	// Call parent first for clustering the applications
	SlowfirstClustered::apply(current_interval, tasklist);

	LOGDEB(fmt::format("function: {}, interval: {}", __PRETTY_FUNCTION__, current_interval));

	auto diffs = std::vector<uint64_t>(clusters.size());
	auto linear = std::vector<double>(clusters.size());
	auto quadratic = std::vector<double>(clusters.size());
	auto exponential = std::vector<double>(clusters.size());

	for (size_t i = 0; i < clusters.size(); i++)
	{
		diffs[i] = clusters[i].getCentroid()[0];
		if (i < clusters.size() - 1)
			diffs[i] -= clusters[i + 1].getCentroid()[0];

		linear[i] = diffs[i] / clusters[0].getCentroid()[0];
		quadratic[i] = pow(linear[i], 2);
		exponential[i] = exp(linear[i]);
	}

	LOGDEB("Intercluster distances:");
	LOGDEB(diffs);

	double ltot = 0;
	double qtot = 0;
	double etot = 0;
	for (double &x : linear) ltot += x;
	for (double &x : quadratic) qtot += x;
	for (double &x : exponential) etot += x;

	LOGDEB("Lineal, quadratic and exponential models:");
	LOGDEB(linear);
	LOGDEB(quadratic);
	LOGDEB(exponential);

	LOGDEB("Teoretical ways:");
	for (double &x : linear) x = x / ltot * max_num_ways;
	for (double &x : quadratic) x = x / qtot * max_num_ways;
	for (double &x : exponential) x = x / etot * max_num_ways;
	LOGDEB(linear);
	LOGDEB(quadratic);
	LOGDEB(exponential);

	for (auto v : {&linear, &quadratic, &exponential})
	{
		double p = max_num_ways;
		for (size_t i = 0; i < v->size(); i++)
		{
			auto &x = (*v)[i];
			x = p - x;
			p = x;
		}
		for (size_t i = v->size() - 1; i > 0; i--)
			(*v)[i] = std::max((uint32_t) round((*v)[i - 1]), cat::min_num_ways);
		(*v)[0] = max_num_ways;
	}

	LOGDEB("Effective ways:");
	LOGDEB(linear);
	LOGDEB(quadratic);
	LOGDEB(exponential);

	for (size_t i = 0;  i < clusters.size(); i++)
	{
		size_t cos = masks.size() - i - 1;
		uint32_t ways = exponential[i];
		masks[cos] = (complete_mask >> (max_num_ways - ways));
	}

	LOGDEB("Classes Of Service:");
	for (size_t cos = masks.size() - 1; cos < masks.size(); cos--)
		LOGDEB(fmt::format("{{COS{}: {{mask: {:#x}, num_ways: {}}}}}", cos, masks[cos], __builtin_popcount(masks[cos])));

	set_masks(masks);
}


// We want to slit an area of size max_num_ways in clusters.size() partitions.
// This partitions have a minimum size of min_num_ways ans a maximum size of max_num_ways.
// We will use 3 different models: a linear, a quadratic, and an exponential model.
// Therefore we will have 3 different functions (fl, fq fe), defined for the intervals [0, al], [0, aq], and [0, ae].
// Each one fulfills fn(0) == min_num_ways and fn(an) == max_num_ways and grows linearli, quadratically and exponentially, respectively.
SfCOA::Model::Model(const std::string &name) : name(name)
{
	models =
	{
		{ "none", [](double x) -> double
			{
				assert(x >= 0 && x <= 1);
				throw_with_trace(std::runtime_error("The 'none' model is not suposed to be called"));
			}
		},
		{ "linear", [](double x) -> double
			{
				assert(x >= 0 && x <= 1);
				const double a = max_num_ways - min_num_ways;
				x *= a; // Scale X for the interval [0, a]
				return x + min_num_ways;
			}
		},
		{ "quadratic", [](double x) -> double
			{
				assert(x >= 0 && x <= 1);
				const double a = std::sqrt(max_num_ways - min_num_ways);
				x *= a; // Scale X for the interval [0, a]
				return std::pow(x, 2) + min_num_ways;
			}
		},
		{ "exponential", [](double x) -> double
			{
				assert(x >= 0 && x <= 1);
				const double a = std::log(max_num_ways - min_num_ways + 1);
				x *= a; // Scale X for the interval [0, a]
				return std::exp(x) + min_num_ways - 1;
			}
		},
		{ "expquad", [](double x) -> double
			{
				assert(x >= 0 && x <= 1);
				const double a = std::sqrt(std::log(max_num_ways - min_num_ways + 1));
				x *= a; // Scale X for the interval [0, a]
				return std::exp(pow(x, 2)) + min_num_ways - 1;
			}
		},
		{ "log", [](double x) -> double
			{
				assert(x >= 0 && x <= 1);
				const double a = 14.849;
				x *= a; // Scale X for the interval [0, a]
				return 15 * std::log(x + 1) + min_num_ways;
			}
		},
		{ "linlog", [](double x) -> double
			{
				assert(x >= 0 && x <= 1);
				const double a = 15.222;
				x *= a; // Scale X for the interval [0, a]
				return x * std::log(x) + 2;
			}
		},
		{ "camel", [](double x) -> double
			{
				assert(x >= 0 && x <= 1);
				const double a = 21.522;
				x *= a; // Scale X for the interval [0, a]
				return (0.9 * x - 25) * std::exp(0.1 * x) + 0.005 * std::pow(x + 40, 2) + x + 24;
			}
		},
	};

	if (models.count(name) == 0)
		throw_with_trace(std::runtime_error("Unwnown model '{}'"_format(name)));

	model = models[name];
}


void SlowfirstClusteredOptimallyAdjusted::apply(uint64_t current_interval, const std::vector<Task> &tasklist)
{
	// Apply the policy only when the amount of intervals specified has passed
	if (current_interval % every != 0)
		return;

	auto data = std::vector<Point>();
	acc::accumulator_set<double, acc::stats<acc::tag::mean, acc::tag::variance, acc::tag::max>> accum;

	LOGDEB(fmt::format("function: {}, interval: {}", __PRETTY_FUNCTION__, current_interval));

	// Put data in the format KMeans expects
	LOGDEB("Tasks:");
	for (const auto &task : tasklist)
	{
		uint64_t l3_hits;
		uint64_t l3_misses;
		uint64_t stalls;
		uint64_t accum_stalls;
		const std::string he = "MEM_LOAD_UOPS_RETIRED.L3_HIT";
		const std::string me = "MEM_LOAD_UOPS_RETIRED.L3_MISS";
		const std::string se = "CYCLE_ACTIVITY.STALLS_TOTAL";
		try
		{
			l3_hits = acc::rolling_mean(task.stats_interval.events.at(he));
			l3_misses = acc::rolling_mean(task.stats_interval.events.at(me));
			stalls = acc::rolling_mean(task.stats_interval.events.at(se));
			accum_stalls = acc::sum(task.stats_interval.events.at(se));
		}
		catch (const std::exception &e)
		{
			std::string msg = "This policy requires the events {}, {} and {}. The events monitorized are:"_format(he, me, se);
			for (const auto &kv : task.stats_total.events)
				msg += "\n" + kv.first;
			throw_with_trace(std::runtime_error(msg));
		}

		double hr = (double) l3_hits / (double) (l3_hits + l3_misses);
		double metric = accum_stalls;

		double completed = task.max_instr ?
				(double) task.stats_total.instructions / (double) task.max_instr : task.completed;

		accum(stalls);

		data.push_back(Point(task.id, {metric}));
		LOGDEB(fmt::format("{{id: {}, executable: {}, completed: {:.2f}, metric: {}, stalls: {:n}, hr: {}}}", task.id, task.executable, completed, metric, stalls, hr));
	}

	if (min_stall_ratio > 0)
	{
		// Ratio of cycles stalled and cycles the execution has been running for the most stalled application
		const double stall_ratio = acc::max(accum) / tasklist[0].stats_total.invariant_cycles;
		if (stall_ratio < min_stall_ratio)
		{
			LOGDEB("Better to do nothing, since the processor is only stalled {}% of the time"_format(stall_ratio * 100));
			cat.reset();
			return;
		}
	}

	if (num_clusters > 0)
	{
		LOGDEB("Enforce {} clusters..."_format(num_clusters));
		KMeans::clusterize(num_clusters, data, clusters, 100);
	}
	else
	{
		assert(num_clusters == 0);

		if (cluster_sizes.size() > 0)
		{
			LOGDEB("Use predetermined cluster sizes...");
			// Sort points in DESCENDING order
			std::sort(begin(data), end(data),
					[](const Point &p1, const Point &p2)
					{
						return p1.values[0] > p2.values[0];
					});
			clusters.clear();
			size_t cid = 0;
			auto data_iter = data.begin();
			for (const auto &size : cluster_sizes)
			{
				clusters.push_back(Cluster(cid++, {0}));
				for (size_t i = 0; i < size; i++)
				{
					assert(data_iter != data.end());
					const Point &point = *data_iter;
					clusters.back().addPoint(&point);
					data_iter++;
				}
				clusters.back().updateMeans();
			}
		}
		else if (min_max)
		{
			LOGDEB("Make min max pairs...");
			assert(data.size() % 2 == 0 && (data.size() / 2) <= cat.get_max_num_cos());
			// Sort points in DESCENDING order
			std::sort(begin(data), end(data),
					[](const Point &p1, const Point &p2)
					{
						return p1.values[0] > p2.values[0];
					});
			clusters.clear();
			size_t cid = 0;
			auto it1 = data.begin();
			auto it2 = data.end();
			it2--;
			while (std::distance(data.begin(), it1) < std::distance(data.begin(), it2))
			{
				const Point &p1 = *it1;
				const Point &p2 = *it2;
				clusters.push_back(Cluster(cid++, {0}));
				clusters.back().addPoint(&p1);
				clusters.back().addPoint(&p2);
				clusters.back().updateMeans();
				it1++;
				it2--;
			}
		}
		else
		{
			LOGDEB("Try to find the optimal number of clusters...");
			KMeans::clusterize_optimally(cat.get_max_num_cos(), data, clusters, 100, eval_clusters);
		}
	}

	LOGDEB(fmt::format("Clusterize: {} points in {} clusters", data.size(), clusters.size()));

	// Sort clusters in DESCENDING order
	std::sort(begin(clusters), end(clusters),
			[](const Cluster &c1, const Cluster &c2)
			{
				return c1.getCentroid()[0] > c2.getCentroid()[0];
			});

	LOGDEB("Sorted clusters:");
	for (const auto &cluster : clusters)
		LOGDEB(cluster.to_string());

	LOGDEB("Selected model: {}"_format(model.name));
	if (model.name != "none")
	{
		for (size_t i = 0; i < clusters.size(); i++)
		{
			const double x = clusters[i].getCentroid()[0] / clusters.front().getCentroid()[0];
			const double y = model(x);
			LOGDEB("Cluster {} : x = {} y = {} -> {} ways"_format(i, x, y, std::round(y)));
		}
	}

	// Get current CAT masks
	masks.resize(cat.get_max_num_cos());
	for (size_t cos = 0; cos < masks.size(); cos++)
		masks[cos] = cat.get_cos_mask(cos);

	// Compute new masks
	if (model.name != "none")
	{
		const double th = acc::mean(accum) + 2 * std::sqrt(acc::variance(accum));
		const Point *p = *std::max_element(clusters[1].getPoints().begin(), clusters[1].getPoints().end(), [](const Point *a, const Point *b){ return a->values[0] < b->values[0]; });

		size_t c;
		for (c = 0;  c < clusters.size(); c++)
		{
			double quotient = clusters.front().getCentroid()[0];
			if (detect_outliers && quotient > th && clusters[0].getPoints().size() == 1)
			{
				quotient = p->values[0]; // The max in the second cluster
				LOGDEB("The most slowed-down application seems an outlayer... use {} instead of {}, {}"_format(quotient, clusters.front().getCentroid()[0], quotient / clusters.front().getCentroid()[0]));
			}

			const double x = std::min(clusters[c].getCentroid()[0] / quotient, 1.0);
			const uint32_t ways = std::round(model(x));
			if (alternate_sides && (c % 2) == 1)
				masks[c] = (complete_mask << (max_num_ways - ways)) & complete_mask; // The mask starts from the left
			else
				masks[c] = (complete_mask >> (max_num_ways - ways)); // The mask starts from the right
		}
		for (;  c < masks.size(); c++)
			masks[c] = complete_mask;
		set_masks(masks);
	}

	LOGDEB("Classes Of Service:");
	for (size_t c = 0; c < masks.size(); c++)
	{
		std::string task_ids;
		if (c < clusters.size())
		{
			size_t i = 0;
			for (const auto &p : clusters[c].getPoints())
			{
				const Task &task = tasklist[p->id];
				cat.set_cos_cpu(c, task.cpu);
				task_ids += std::to_string(p->id);
				task_ids += (i == clusters[c].getPoints().size() - 1) ? "" : ", ";
				i++;
			}
		}
		LOGDEB(fmt::format("{{COS{}: {{mask: {:#7x}, num_ways: {:2}, tasks: [{}]}}}}", c, masks[c], __builtin_popcount(masks[c]), task_ids));
	}
}


}} // cat::policy
