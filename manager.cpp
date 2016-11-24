#include <clocale>
#include <iostream>
#include <thread>

#include <boost/program_options.hpp>
#include <yaml-cpp/yaml.h>

#include "cat-policy.hpp"
#include "common.hpp"
#include "config.hpp"
#include "events-intel.hpp"
#include "log.hpp"
#include "stats.hpp"
#include "task.hpp"


// Some predefined events, used when no events are supplied, but only tested in the Intel Xeon e5 2658A v3, so be carefull
#define L1_HITS   "cpu/umask=0x01,event=0xD1,name=MEM_LOAD_UOPS_RETIRED.L1_HIT/"
#define L1_MISSES "cpu/umask=0x08,event=0xD1,name=MEM_LOAD_UOPS_RETIRED.L1_MISS/"
#define L2_HITS   "cpu/umask=0x02,event=0xD1,name=MEM_LOAD_UOPS_RETIRED.L2_HIT/"
#define L2_MISSES "cpu/umask=0x10,event=0xD1,name=MEM_LOAD_UOPS_RETIRED.L2_MISS/"
#define L3_HITS   "cpu/umask=0x04,event=0xD1,name=MEM_LOAD_UOPS_RETIRED.L3_HIT/"
#define L3_MISSES "cpu/umask=0x20,event=0xD1,name=MEM_LOAD_UOPS_RETIRED.L3_MISS/"
#define STALLS_L2_PENDING  "cpu/umask=0x05,event=0xA3,name=CYCLE_ACTIVITY.STALLS_L2_PENDING,cmask=5/"
#define STALLS_LDM_PENDING "cpu/umask=0x06,event=0xA3,name=CYCLE_ACTIVITY.STALLS_LDM_PENDING,cmask=6/"


namespace po = boost::program_options;
namespace chr = std::chrono;

using std::string;
using std::to_string;
using std::vector;
using std::this_thread::sleep_for;
using std::cout;
using std::endl;


CAT cat_setup(const vector<Cos> &coslist, bool auto_reset);
void loop(vector<Task> &tasklist, std::shared_ptr<CAT_Policy> catpol, const vector<string> &events, uint64_t time_int_us, uint32_t max_int, std::ostream &out, std::ostream &fin_out);
void clean(vector<Task> &tasklist, CAT &cat);
[[noreturn]]
void clean_and_die(vector<Task> &tasklist, CAT &cat);
std::string program_options_to_string(const std::vector<po::option>& raw);


CAT cat_setup(const vector<Cos> &coslist, bool auto_reset)
{
	auto cat = CAT(auto_reset); // If auto_reset is set CAT is reseted before and after execution
	cat.init();

	for (size_t i = 0; i < coslist.size(); i++)
	{
		const auto &cos = coslist[i];
		cat.set_cos_mask(i, cos.mask);
		for (const auto &cpu : cos.cpus)
			cat.set_cos_cpu(i, cpu);
	}

	return cat;
}


void loop(vector<Task> &tasklist, std::shared_ptr<CAT_Policy> catpol, const vector<string> &events, uint64_t time_int_us, uint32_t max_int, std::ostream &out, std::ostream &fin_out)
{
	if (time_int_us <= 0)
		throw std::runtime_error("Interval time must be positive and greater than 0");
	if (max_int <= 0)
		throw std::runtime_error("Max time must be positive and greater than 0");

	// For adjusting the time sleeping
	uint64_t adj_delay_us = time_int_us;
	uint64_t total_elapsed_us = 0;
	const double kp = 0.5;
	const double ki = 0.25;

	// The PERfCountMon class receives 3 lambdas: une for resuming task, other for waiting, and the last one for pausing the tasks.
	// Note that each lambda captures variables by reference, specially the waiter,
	// which captures the time to wait by reference, so when it's adjusted it's not necessary to do anithing.
	auto resume = [&tasklist]()     { tasks_resume(tasklist); };
	auto wait   = [&adj_delay_us]() { sleep_for(chr::microseconds(adj_delay_us));};
	auto pause  = [&tasklist]()     { tasks_pause(tasklist); };
	auto perf = PerfCountMon<decltype(resume), decltype(wait), decltype(pause)> (resume, wait, pause);
	perf.mon_custom_events(events);

	// Print headers
	stats_print_header(out);
	stats_final_print_header(fin_out);

	// Loop
	for (uint32_t interval = 0; interval < max_int; interval++)
	{
		auto cores         = tasks_cores_used(tasklist);
		auto stats         = vector<Stats>();
		bool all_completed = true; // Have all the tasks reached their execution limit?

		// Run for some time, take stats and pause
		uint64_t elapsed_us = perf.measure(cores, stats);
		total_elapsed_us += elapsed_us;

		// Process tasks...
		for (size_t i = 0; i < tasklist.size(); i++)
		{
			auto &task = tasklist[i];

			// Deal with PCM transcient errors
			if (stats_are_wrong(stats[i]) && task.stats_acumulated.instructions > 0)
			{
				LOGWAR("The results for the interval " << interval << " seem wrong:");
				LOGWAR(stats_header_to_string(" "));
				LOGWAR(stats_to_string(stats[i], task.cpu, task.id, task.executable, interval, " "));
				LOGWAR("They will be replaced with the values from the previous interval:");
				LOGWAR(stats_to_string(task.stats_interval, task.cpu, task.id, task.executable,  interval - 1, " "));
				stats[i] = task.stats_interval;
			}

			// Count stats
			task.stats_interval    = stats[i];
			task.stats_acumulated += stats[i];
			task.stats_total      += stats[i];

			// Instruction limit reached
			if (task.stats_acumulated.instructions >= task.max_instr)
			{
				task.limit_reached = true;

				// It's the first time it reaches the limit
				if (!task.completed)
				{
					task.completed = true;

					// Print interval stats for the last time
					stats_print(task.stats_interval, out, task.cpu, task.id, task.executable, interval);

					// Print acumulated stats
					stats_print(task.stats_acumulated, fin_out, task.cpu, task.id, task.executable);
				}
			}

			// This task has never reached any limits
			if (!task.completed)
			{
				// Batch tasks do not need to be completed
				if (!task.batch)
					all_completed = false;
				stats_print(task.stats_interval, out, task.cpu, task.id, task.executable, interval);
			}
		}

		// All the tasks have reached their limit -> finish execution
		if (all_completed)
			break;

		// Restart the ones that have reached their limit
		else
			tasks_kill_and_restart(tasklist);

		// Adjust CAT according to the selected policy
		catpol->adjust(interval, tasklist);

		// Adjust time with a PI controller
		int64_t proportional = (int64_t) time_int_us - (int64_t) elapsed_us;
		int64_t integral     = (int64_t) time_int_us * (interval + 1) - (int64_t) total_elapsed_us;
		adj_delay_us += kp * proportional;
		adj_delay_us += ki * integral;
	}

	// Print acumulated stats for non completed tasks
	for (const auto &task : tasklist)
	{
		if (!task.completed)
			stats_print(task.stats_acumulated, fin_out, task.cpu, task.id, task.executable);
	}

	// For some reason killing a stopped process returns EPERM... this solves it
	tasks_resume(tasklist);
}


// Leave the machine in a consistent state
void clean(vector<Task> &tasklist, CAT &cat)
{
	cat.cleanup();
	pcm_cleanup();

	// Try to drop privileges before killing anything
	drop_privileges();

	for (auto &task : tasklist)
		task_kill(task);
}


void clean_and_die(vector<Task> &tasklist, CAT &cat)
{
	LOGERR("--- PANIC, TRYING TO CLEAN ---");

	try
	{
		if (cat.is_initialized())
			cat.reset();
	}
	catch (const std::exception &e)
	{
		LOGERR("Could not reset CAT: " << e.what());
	}

	try
	{
		pcm_reset();
	}
	catch (const std::exception &e)
	{
		LOGERR("Could not reset PCM: " << e.what());
	}

	for (auto &task : tasklist)
	{
		try
		{
			if (task.pid > 0)
				task_kill(task);
		}
		catch (const std::exception &e)
		{
			LOGERR(e.what());
		}
	}

	LOGFAT("Exit with error");
}


std::string program_options_to_string(const std::vector<po::option>& raw)
{
    string args;

    for (const po::option& option: raw)
    {
        // if(option.unregistered) continue; // Skipping unknown options

        if(option.value.empty())
            args += option.string_key + "\n";
        else
        {
            // this loses order of positional options
            for (const std::string& value : option.value)
            {
                args += option.string_key + ": ";
                args += value + "\n";
            }
        }
    }

    return args;
}


int main(int argc, char *argv[])
{
	srand(time(NULL));

	// Set the locale to the one defined in the corresponding enviroment variable
	std::setlocale(LC_ALL, "");

	// Default log conf
	const string logfile = "manager.log";
	const string min_clog = "war";
	const string min_flog = "inf";
	general_log::init(logfile, general_log::severity_level(min_clog), general_log::severity_level(min_flog));

	po::options_description desc("Allowed options");
	desc.add_options()
		("help,h", "print usage message")
		("config,c", po::value<string>()->required(), "pathname for yaml config file")
		("output,o", po::value<string>(), "pathname for output")
		("fin-output", po::value<string>(), "pathname for output values when tasks are completed")
		("rundir", po::value<string>()->default_value("run"), "directory for creating the directories where the applications are gonna be executed")
		("id", po::value<string>()->default_value(random_string(5)), "identifier for the experiment")
		("ti", po::value<double>()->default_value(1), "time-interval, duration in seconds of the time interval to sample performance counters.")
		("mi", po::value<uint32_t>()->default_value(std::numeric_limits<uint32_t>::max()), "max-intervals, maximum number of intervals.")
		("event,e", po::value<vector<string>>()->composing()->multitoken(), "optional list of custom events to monitor (up to 4)")
		("cpu-affinity", po::value<vector<uint32_t>>()->multitoken(), "cpus in which this application (not the workloads) is allowed to run")
		("reset-cat", po::value<bool>()->default_value(true), "reset CAT config, before and after the program execution. Note that even if this is false a CAT policy may reset the CAT config during it's normal operation.")
		("clog-min", po::value<string>()->default_value(min_clog), "Minimum severity level to log into the console, defaults to warning")
		("flog-min", po::value<string>()->default_value(min_flog), "Minimum severity level to log into the log file, defaults to info")
		("log-file", po::value<string>()->default_value(logfile), "file used for the general application log")
		;

	bool option_error = false;
	string options;
	po::variables_map vm;
	try
	{
		// Parse the options without storing them in a map.
		po::parsed_options parsed_options = po::command_line_parser(argc, argv)
			.options(desc)
			.run();
		options = program_options_to_string(parsed_options.options);

		po::store(parsed_options, vm);
		po::notify(vm);
	}
	catch(const std::exception &e)
	{
		LOGERR(e.what());
		option_error = true;
	}

	if (vm.count("help") || option_error)
	{
		cout << desc << endl;
		exit(option_error ? EXIT_FAILURE : EXIT_SUCCESS);
	}

	// Log init with user defined parameters
	general_log::init(
			vm["log-file"].as<string>(),
			general_log::severity_level(vm["clog-min"].as<string>()),
			general_log::severity_level(vm["flog-min"].as<string>()));

	// Log the program options
	LOGINF("Program options:\n" + options);

	// Set CPU affinity for not interfering with the executed workloads
	if (vm.count("cpu-affinity"))
		set_cpu_affinity(vm["cpu-affinity"].as<vector<uint32_t>>());

	// Open output file if needed; if not, use cout
	auto file1 = std::ofstream();
	if (vm.count("output"))
		file1 = open_ofstream(vm["output"].as<string>());
	std::ostream &out = file1.is_open() ? file1 : cout;

	// Output file for final output
	auto file2 = std::ofstream();
	auto ss_aux = std::stringstream();
	if (vm.count("fin-output"))
		file2 = open_ofstream(vm["fin-output"].as<string>());
	std::ostream &fin_out = file2.is_open() ? file2 : dynamic_cast<std::ostream &>(ss_aux);

	// Read config
	auto tasklist = vector<Task>();
	auto coslist = vector<Cos>();
	auto catpol = std::make_shared<CAT_Policy>(); // We want to use polimorfism, so we need a pointer
	string config_file;
	try
	{
		// Read config and set tasklist and coslist
		config_file = vm["config"].as<string>();
		config_read(config_file, tasklist, coslist, catpol);
		tasks_set_rundirs(tasklist, vm["rundir"].as<string>() + "/" + vm["id"].as<string>());
	}
	catch(const YAML::ParserException &e)
	{
		LOGFAT(string("Error in config file in line: ") + to_string(e.mark.line) + " col: " + to_string(e.mark.column) + " pos: " + to_string(e.mark.pos) + ": " + e.msg);
	}
	catch(const std::exception &e)
	{
		LOGFAT("Error reading config file '" + config_file + "': " + e.what());
	}

	try
	{
		// Initial CAT configuration. It may be modified by the CAT policy.
		CAT cat = cat_setup(coslist, vm["reset-cat"].as<bool>());
		catpol->set_cat(cat);

		pcm_reset();
	}
	catch (const std::exception &e)
	{
		LOGFAT(e.what());
	}

	try
	{
		// Events to monitor
		auto events = vector<string>{L2_HITS, L2_MISSES, L3_HITS, L3_MISSES};
		if (vm.count("event"))
			events = vm["event"].as<vector<string>>();

		// Execute and immediately pause tasks
		for (auto &task : tasklist)
			task_execute(task);

		// Start doing things
		loop(tasklist, catpol, events, vm["ti"].as<double>() * 1000 * 1000, vm["mi"].as<uint32_t>(), out, fin_out);

		// If no --fin-output argument, then the final stats are buffered in a stringstream and then outputted to stdout.
		// If we don't do this and the normal output also goes to stdout, they would mix.
		if (!vm.count("fin-output"))
			cout << dynamic_cast<std::stringstream &>(fin_out).str();

		// Kill tasks, reset CAT, performance monitors, etc...
		clean(tasklist, catpol->get_cat());
	}
	catch(const std::exception &e)
	{
		LOGERR(e.what());
		clean_and_die(tasklist, catpol->get_cat());
	}
}
