#include <iomanip>
#include <sstream>

#include <boost/io/ios_state.hpp>

#include "stats.hpp"


#define WIN_SIZE 7


namespace acc = boost::accumulators;


Stats::Stats(const Measurement &m) :
		us(m.us),
		instructions(m.instructions),
		cycles(m.cycles),
		invariant_cycles(m.invariant_cycles),
		ipc((double) m.instructions / (double) m.cycles),
		ipnc((double) m.instructions / (double) m.invariant_cycles),
		rel_freq(m.rel_freq),
		act_rel_freq(m.act_rel_freq),
		l3_kbytes_occ(m.l3_kbytes_occ),
		mc_gbytes_rd(m.mc_gbytes_rd),
		mc_gbytes_wt(m.mc_gbytes_wt),
		proc_energy(m.proc_energy),
		dram_energy(m.dram_energy)
{
	for (const auto &kv : m.events)
	{
		events.insert(std::make_pair(kv.first, accum_t(acc::tag::rolling_window::window_size = WIN_SIZE)));
		events.at(kv.first)(kv.second);
	}
}


Stats& Stats::accum(const Measurement &m)
{
	// Compute this metrics before modifying other things
	rel_freq     = ((rel_freq * invariant_cycles) + (m.rel_freq * m.invariant_cycles)) / (invariant_cycles + m.invariant_cycles); // Weighted mean
	act_rel_freq = ((act_rel_freq * invariant_cycles) + (m.act_rel_freq * m.invariant_cycles)) / (invariant_cycles + m.invariant_cycles); // Weighted mean

	// Weighted mean, which assumes that all the interval had the same occupancy,
	// which may be not true, because we only know the final result, but...
	l3_kbytes_occ = ((l3_kbytes_occ * invariant_cycles) + (m.l3_kbytes_occ * m.invariant_cycles)) / (invariant_cycles + m.invariant_cycles);

	us               += m.us;
	instructions     += m.instructions;
	cycles           += m.cycles;
	invariant_cycles += m.invariant_cycles;
	ipc              = (double) instructions / (double) cycles;     // We could also do a weighted mean
	ipnc             = (double) instructions / (double) invariant_cycles; // We could also do a weighted mean
	mc_gbytes_rd     += m.mc_gbytes_rd;
	mc_gbytes_wt     += m.mc_gbytes_wt;
	proc_energy      += m.proc_energy;
	dram_energy      += m.dram_energy;
	if (events.size() == 0)
	{
		for (const auto &kv : m.events)
			events.insert(std::make_pair(kv.first, accum_t(acc::tag::rolling_window::window_size = WIN_SIZE)));
	}
	assert(m.events.size() == events.size());
	for (const auto &kv : m.events)
		events.at(kv.first)(kv.second);
	return *this;

}


std::string stats_final_header_to_string(const Stats &s, const std::string &sep)
{
	std::string result =
			"app"              + sep +
			"us"               + sep +
			"instructions"     + sep +
			"cycles"           + sep +
			"invariant_cycles" + sep +
			"ipc"              + sep +
			"ipnc"             + sep +
			"rel_freq"         + sep +
			"act_rel_freq"     + sep +
			"l3_kbytes_occ"    + sep +
			"mc_gbytes_rd"     + sep +
			"mc_gbytes_wt"     + sep +
			"proc_energy"      + sep +
			"dram_energy";

	for (const auto &kv : s.events)
	{
		result += sep;
		result += kv.first;
	}
	return result;
}


std::string stats_header_to_string(const Stats &s, const std::string &sep)
{
	return "interval" + sep + stats_final_header_to_string(s, sep);
}


std::string stats_to_string(const Stats &s, uint32_t id, const std::string &app, uint64_t interval, const std::string &sep)
{
	std::ostringstream out;
	if (interval != -1ULL)
		out << interval       << sep << std::setfill('0') << std::setw(2);
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
	out << s.dram_energy;
	for (const auto &kv : s.events)
	{
		out << sep;
		out << acc::sum(kv.second);
	}
	return out.str();
}


void stats_final_print_header(const Stats &s, std::ostream &out, const std::string &sep)
{
	out << stats_final_header_to_string(s, sep) << std::endl;
}


void stats_print_header(const Stats &s, std::ostream &out, const std::string &sep)
{
	out << stats_header_to_string(s, sep) << std::endl;
}


void stats_print(const Stats &s, std::ostream &out, uint32_t id, const std::string &app, uint64_t interval, const std::string &sep)
{
	out << stats_to_string(s, id, app, interval, sep) << std::endl;
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


// Detect transcient errors in PCM results
// They happen sometimes... Good work, Intel.
bool measurements_are_wrong(const Measurement &m)
{
	const double ipc = (double) m.instructions / (double) m.cycles;
	const double ipnc = (double) m.instructions / (double) m.invariant_cycles;
	const double approx_mhz = (double) m.cycles / (double) m.us;
	const double approx_inv_mhz = (double) m.cycles / (double) m.us;

	if (approx_mhz > 10000 || approx_inv_mhz > 10000)
		return true;

	if (ipnc > 100 || ipnc < 0)
		return true;

	if (ipc > 100 || ipc < 0)
		return true;

	if (m.rel_freq < 0 || m.act_rel_freq < 0)
		return true;

	if (m.l3_kbytes_occ > 1000000) // More than one GB...
		return true;

	if (m.mc_gbytes_rd < 0 || m.mc_gbytes_wt < 0)
		return true;

	if (m.proc_energy < 0 || m.dram_energy < 0)
		return true;

	return false;
}
