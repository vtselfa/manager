#pragma once

#include <chrono>
#include <iostream>
#include <vector>

#include "intel-pcm/cpucounters.h"
#include "stats.hpp"


// Prototipes
struct EventSelectRegister;
struct CoreEvent;

void pcm_build_event(const char *event_str, EventSelectRegister &reg, CoreEvent &event);
void pcm_cleanup();
void pcm_reset();


// Structures
struct CoreEvent
{
	char     name[256];
	uint64_t value;
	uint64_t msr_value;
	char    *description;
};


template <typename Resume, typename Wait, typename Pause>
class PerfCountMon
{
	PCM *m = nullptr;

	// Callable template attributes
	Resume resume;
	Wait wait;
	Pause pause;

	public:

	PerfCountMon() = delete;

	PerfCountMon(Resume r, Wait w, Pause p) : m(PCM::getInstance()), resume(r), wait(w), pause(p) {}


	void clean()
	{
		m->cleanup();
	}


	void reset()
	{
		m->resetPMU();
	}


	void check_status(PCM::ErrorCode status)
	{
		switch (status)
		{
			case PCM::Success:
				break;

			case PCM::MSRAccessDenied:
				throw std::runtime_error("Access to PMU denied: No MSR or PCI CFG space access");

			case PCM::PMUBusy:
				throw std::runtime_error("Access to PMU denied: The Performance Monitoring Unit is occupied by another application");

			default:
				throw std::runtime_error("Access to PMU denied: unknown error)");
		}
	}


	void mon_custom_events(const std::vector<std::string> &str_events)
	{
		CoreEvent           events[MAX_EVENTS];
		EventSelectRegister regs[MAX_EVENTS];

		PCM::ExtendedCustomCoreEventDescription conf;

		if (str_events.size() > MAX_EVENTS)
			throw std::runtime_error("At most " + std::to_string(MAX_EVENTS) + " events are allowed");

		// Build events
		for (size_t i = 0; i < str_events.size(); i++)
			pcm_build_event(str_events[i].c_str(), regs[i], events[i]);

		// Prepare conf
		conf.fixedCfg = NULL; // default
		conf.nGPCounters = 4;
		conf.gpCounterCfg = regs;
		conf.OffcoreResponseMsrValue[0] = events[0].msr_value;
		conf.OffcoreResponseMsrValue[1] = events[1].msr_value;

		// Program PCM unit
		PCM::ErrorCode status = m->program(PCM::EXT_CUSTOM_CORE_EVENTS, &conf);

		check_status(status);
	}


	// Takes snapshot, works, takes another snapshot, and returns the results
	uint64_t measure(const std::vector<uint32_t> &cores, std::vector<Stats> &results)
	{
		namespace chr = std::chrono;

		auto sb = SystemCounterState();
		auto sa = SystemCounterState();
		auto cb = std::vector<CoreCounterState>(m->getNumCores());
		auto ca = std::vector<CoreCounterState>(m->getNumCores());
		auto skb = std::vector<SocketCounterState>(m->getNumSockets());
		auto ska = std::vector<SocketCounterState>(m->getNumSockets());

		results.clear();

		// Resume applications
		resume();

		// Collect stats
		auto start = chr::system_clock::now();
		m->getCounterStates(cores, sb, skb, cb);

		// Wait some time
		wait();

		// Collect stats
		m->getCounterStates(cores, sa, ska, ca);
		uint64_t duration = chr::duration_cast<chr::microseconds>
			(chr::system_clock::now() - start).count();

		// Pause applications
		pause();

		// Process stats
		for (const auto &core : cores)
		{
			if (!m->isCoreOnline(core))
				throw std::runtime_error("Core " + std::to_string(core) + " is not online");

			Stats stats =
			{
				.us               = duration,
				.instructions     = getInstructionsRetired(cb[core], ca[core]),
				.cycles           = getCycles(cb[core], ca[core]),
				.invariant_cycles = getInvariantTSC(cb[core], ca[core]),
				.ipc              = getIPC(cb[core], ca[core]),
				.ipnc             = getExecUsage(cb[core], ca[core]),
				.rel_freq         = getRelativeFrequency(cb[core], ca[core]),
				.act_rel_freq     = getActiveRelativeFrequency(cb[core], ca[core]),
				.l3_kbytes_occ    = getL3CacheOccupancy(ca[core]),
				.mc_gbytes_rd     = getBytesReadFromMC(sb, sa) / double(1e9),
				.mc_gbytes_wt     = getBytesWrittenToMC(sb, sa) / double(1e9),
				.proc_energy      = getConsumedJoules(sb, sa),     // Energy conumed by the processor, excluding the DRAM
				.dram_energy      = getDRAMConsumedJoules(sb, sa), // Energy consumed by the DRAM
			};
			for (int i = 0; i < MAX_EVENTS; ++i)
				stats.event[i] = getNumberOfCustomEvents(i, cb[core], ca[core]);

			results.push_back(stats);
		}

		return duration;
	}
};
