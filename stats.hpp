#pragma once

#include <cstdint>
#include <ostream>
#include <vector>


// Global variables
const int MAX_EVENTS = 4;


struct Stats
{
	// Core stats
	uint64_t us; // Microseconds
	uint64_t instructions;
	uint64_t cycles;
	uint64_t invariant_cycles;
	double ipc;
	double ipnc;     // Intructions per nominal cycles
	double rel_freq; // Frequency relative to the nominal CPU frequency
	double act_rel_freq;
	uint64_t l3_kbytes_occ;

	// System stats i.e. equal for all the cores
	double mc_gbytes_rd; // In GB
	double mc_gbytes_wt; // In GB
	double proc_energy;  // In joules
	double dram_energy;  // In joules

	// Core events
	std::vector<uint64_t> event;

	Stats() : event(MAX_EVENTS, 0) {}

	Stats
	(
			uint64_t us,
			uint64_t instructions,
			uint64_t cycles,
			uint64_t invariant_cycles,
			double ipc,
			double ipnc,
			double rel_freq,
			double act_rel_freq,
			uint64_t l3_kbytes_occ,
			double mc_gbytes_rd,
			double mc_gbytes_wt,
			double proc_energy,
			double dram_energy,
			const std::vector<uint64_t> &event
	) :
			us(us),
			instructions(instructions),
			cycles(cycles),
			invariant_cycles(invariant_cycles),
			ipc(ipc),
			ipnc(ipnc),
			rel_freq(rel_freq),
			act_rel_freq(act_rel_freq),
			l3_kbytes_occ(l3_kbytes_occ),
			mc_gbytes_rd(mc_gbytes_rd),
			mc_gbytes_wt(mc_gbytes_wt),
			proc_energy(proc_energy),
			dram_energy(dram_energy),
			event(event)
	{}

	Stats& operator+=(const Stats &o);
	friend Stats operator+(Stats a, const Stats &b);
};


std::string stats_final_header_to_string(const std::string &sep=",");
std::string stats_header_to_string(const std::string &sep=",");
std::string stats_to_string(const Stats &s, uint32_t cpu, uint32_t id, const std::string &app, uint64_t interval, const std::string &sep=",");
void stats_final_print_header(std::ostream &out, const std::string &sep=",");
void stats_print_header(std::ostream &out, const std::string &sep=",");
void stats_print(const Stats &s, std::ostream &out, uint32_t cpu, uint32_t id, const std::string &app, uint64_t interval = -1ULL, const std::string &sep=",");
bool stats_are_wrong(const Stats &s);
