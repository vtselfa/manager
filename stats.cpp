#include <iomanip>

#include <boost/io/ios_state.hpp>

#include "stats.hpp"


Stats& Stats::operator+=(const Stats &o)
{
	// Compute this metrics before modifying other things
	rel_freq     = ((rel_freq * invariant_cycles) + (o.rel_freq * o.invariant_cycles)) / (invariant_cycles + o.invariant_cycles); // Weighted mean
	act_rel_freq = ((act_rel_freq * invariant_cycles) + (o.act_rel_freq * o.invariant_cycles)) / (invariant_cycles + o.invariant_cycles); // Weighted mean

	// Weighted mean, which assumes that all the interval had the same occupancy,
	// which may be not true, because we only know the final result, but...
	l3_kbytes_occ = ((l3_kbytes_occ * invariant_cycles) + (o.l3_kbytes_occ * o.invariant_cycles)) / (invariant_cycles + o.invariant_cycles);

	us               += o.us;
	instructions     += o.instructions;
	cycles           += o.cycles;
	invariant_cycles += o.invariant_cycles;
	ipc              = (double) instructions / (double) cycles;     // We could also do a weighted mean
	ipnc             = (double) instructions / (double) invariant_cycles; // We could also do a weighted mean
	mc_gbytes_rd     += o.mc_gbytes_rd;
	mc_gbytes_wt     += o.mc_gbytes_wt;
	proc_energy      += o.proc_energy;
	dram_energy      += o.dram_energy;
	for (int i = 0; i < MAX_EVENTS; i++)
		event[i] += o.event[i];
	return *this;

}


Stats operator+(Stats a, const Stats &b)
{
	a += b;
	return a;
}


void stats_final_print_header(std::ostream &out, const std::string &sep)
{
	out << "core"             << sep;
	out << "app"              << sep;
	out << "us"               << sep;
	out << "instructions"     << sep;
	out << "cycles"           << sep;
	out << "invariant_cycles" << sep;
	out << "ipc"              << sep;
	out << "ipnc"             << sep;
	out << "rel_freq"         << sep;
	out << "act_rel_freq"     << sep;
	out << "l3_kbytes_occ"    << sep;
	out << "mc_gbytes_rd"     << sep;
	out << "mc_gbytes_wt"     << sep;
	out << "proc_energy"      << sep;
	out << "dram_energy"      << sep;

	for (uint32_t i = 0; i < MAX_EVENTS; ++i)
	{
		out << "ev" << i;
		if (i < MAX_EVENTS - 1)
			out << sep;
	}
	out << std::endl;
}


void stats_print_header(std::ostream &out, const std::string &sep)
{
	out << "interval" << sep;
	stats_final_print_header(out);
}


void stats_print(const Stats &s, std::ostream &out, uint32_t cpu, uint32_t id, const std::string &app, uint64_t interval, const std::string &sep)
{
	boost::io::ios_all_saver guard(out); // Saves current flags and format

	if (interval != -1ULL)
		out << interval       << sep;
	out << cpu                << sep << std::setfill('0') << std::setw(2);
	out << id << "_" << app   << sep;
	out << s.us               << sep;
	out << s.instructions     << sep;
	out << s.cycles           << sep;
	out << s.invariant_cycles << sep;
	out << s.ipc              << sep;
	out << s.ipnc             << sep;
	out << s.rel_freq         << sep;
	out << s.act_rel_freq     << sep;
	out << s.l3_kbytes_occ    << sep;
	out << s.mc_gbytes_rd     << sep;
	out << s.mc_gbytes_wt     << sep;
	out << s.proc_energy      << sep;
	out << s.dram_energy      << sep;
	for (uint32_t i = 0; i < MAX_EVENTS; ++i)
	{
		out << s.event[i];
		if (i < MAX_EVENTS - 1)
			out << sep;
	}
	out << std::endl;
}


// Detect transcient errors in PCM results
// They happen sometimes... Good work, Intel.
bool stats_are_wrong(const Stats &s)
{
	const double approx_mhz = (double) s.cycles / (double) s.us;
	const double approx_inv_mhz = (double) s.cycles / (double) s.us;

	if (approx_mhz > 10000 || approx_inv_mhz > 10000)
		return true;

	if (s.ipnc > 100 || s.ipnc < 0)
		return true;

	if (s.ipc > 100 || s.ipc < 0)
		return true;

	if (s.rel_freq < 0 || s.act_rel_freq < 0)
		return true;

	if (s.l3_kbytes_occ > 1000000) // More that one GB...
		return true;

	if (s.mc_gbytes_rd < 0 || s.mc_gbytes_wt < 0)
		return true;

	if (s.proc_energy < 0 || s.dram_energy < 0)
		return true;

	return false;
}
