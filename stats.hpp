#pragma once

#include <cstdint>
#include <ostream>


// Global variables
const int MAX_EVENTS = 4;

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>
#include <boost/accumulators/statistics/rolling_window.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/variance.hpp>


struct Measurement
{
	// Core stats
	uint64_t us = 0; // Microseconds
	uint64_t instructions = 0;
	uint64_t cycles = 0;
	uint64_t invariant_cycles = 0;
	double rel_freq = 0; // Frequency relative to the nominal CPU frequency
	double act_rel_freq = 0;
	uint64_t l3_kbytes_occ = 0;

	// System stats i.e. equal for all the cores
	double mc_gbytes_rd = 0; // In GB
	double mc_gbytes_wt = 0; // In GB
	double proc_energy = 0;  // In joules
	double dram_energy = 0;  // In joules

	// Core events
	uint64_t events[MAX_EVENTS];

	Measurement() = default;
};


struct Stats
{
	// Core stats
	uint64_t us = 0; // Microseconds
	uint64_t instructions = 0;
	uint64_t cycles = 0;
	uint64_t invariant_cycles = 0;
	double ipc = 0;
	double ipnc = 0;     // Intructions per nominal cycles
	double rel_freq = 0; // Frequency relative to the nominal CPU frequency
	double act_rel_freq = 0;
	uint64_t l3_kbytes_occ = 0;

	// System stats i.e. equal for all the cores
	double mc_gbytes_rd = 0; // In GB
	double mc_gbytes_wt = 0; // In GB
	double proc_energy = 0;  // In joules
	double dram_energy = 0;  // In joules

	// Core events
	#define ACC boost::accumulators
	typedef ACC::accumulator_set <
		double,
		ACC::stats <
			ACC::tag::sum,
			ACC::tag::mean,
			ACC::tag::variance,
			ACC::tag::rolling_mean>> accum_t;
	#undef ACC
	std::vector<accum_t> events;

	Stats();
	Stats(const Measurement &m);

	Stats& accum(const Measurement &m);
	friend Stats operator+(Stats a, const Stats &b);
};


std::string stats_final_header_to_string(const std::string &sep=",");
std::string stats_header_to_string(const std::string &sep=",");
std::string stats_to_string(const Stats &s, uint32_t cpu, uint32_t id, const std::string &app, uint64_t interval, const std::string &sep=",");
void stats_final_print_header(std::ostream &out, const std::string &sep=",");
void stats_print_header(std::ostream &out, const std::string &sep=",");
void stats_print(const Stats &s, std::ostream &out, uint32_t cpu, uint32_t id, const std::string &app, uint64_t interval = -1ULL, const std::string &sep=",");
bool stats_are_wrong(const Stats &s);
bool measurements_are_wrong(const Measurement &m);
