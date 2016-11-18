#pragma once

#include <cstdint>
#include <ostream>


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
	uint64_t event[MAX_EVENTS];

	Stats() = default;

	Stats& operator+=(const Stats &o);
	friend Stats operator+(Stats a, const Stats &b);
};


void stats_final_print_header(std::ostream &out, const std::string &sep=",");
void stats_print_header(std::ostream &out, const std::string &sep=",");
void stats_print(const Stats &s, std::ostream &out, uint32_t cpu, uint32_t id, const std::string &app, uint64_t interval = -1ULL, const std::string &sep=",");
bool stats_are_wrong(const Stats &s);
