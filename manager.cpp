#include <atomic>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

#include <fcntl.h>
#include <grp.h>
#include <sched.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/io/ios_state.hpp>
#include <glib.h>
#include <yaml-cpp/yaml.h>

#include "cat-intel.hpp"
#include "common.hpp"
#include "manager_pcm.hpp"


namespace fs = boost::filesystem;
namespace po = boost::program_options;
namespace chr = std::chrono;

using std::string;
using std::to_string;
using std::vector;
using std::this_thread::sleep_for;
using std::cout;
using std::cerr;
using std::endl;

string extract_executable_name(string cmd);

struct Cos
{
	uint64_t mask;         // Ways assigned mask
	vector<uint32_t> cpus; // Associated CPUs

	Cos(uint64_t mask, vector<uint32_t> cpus = {}) : mask(mask), cpus(cpus){}
};


struct Task
{
	// Number of tasks created
	static std::atomic<uint32_t> ID;

	// Set on construction
	uint32_t id;
	const string cmd;
	const string executable;  // Basename of the executable
	vector<int> cpus;         // Allowed cpus
	const string out;         // Stdout redirection
	const string in;          // Stdin redirection
	const string err;         // Stderr redirection
	const string skel;        // Directory containing files and folders to copy to rundir
	const uint64_t max_instr; // Max number of instructions to execute

	string rundir = ""; // Set before executing the task
	int pid       = 0;  // Set after executing the task

	Stats stats_acumulated = Stats(); // From the start of the execution
	Stats stats_interval   = Stats(); // Only last interval

	bool limit_reached = false; // Has the instruction limit been reached?
	bool completed = false;     // Do not print stats

	Task() = delete;
	Task(string cmd, vector<int> cpus, string out, string in, string err, string skel, uint64_t max_instr) :
		id(ID++), cmd(cmd), executable(extract_executable_name(cmd)), cpus(cpus), out(out), in(in), err(err), skel(skel), max_instr(max_instr) {}

	// Reset stats and limit flag
	void reset()
	{
		limit_reached = false;
		stats_acumulated = Stats();
		stats_interval   = Stats();
	}
};
std::atomic<uint32_t> Task::ID(0);


// Returns the executable basename from a commandline
string extract_executable_name(string cmd)
{
	int argc;
	char **argv;

	if (!g_shell_parse_argv(cmd.c_str(), &argc, &argv, NULL))
		throw std::runtime_error("Could not parse commandline '" + cmd + "'");

	string result = fs::basename(argv[0]);
	g_strfreev(argv); // Free the memory allocated for argv

	return result;
}


vector<Cos> config_read_cos(const YAML::Node &config)
{
	YAML::Node cos_section = config["cos"];
	auto result = vector<Cos>();

	if (cos_section.Type() != YAML::NodeType::Sequence)
		throw std::runtime_error("In the config file, the cos section must contain a sequence");

	for (size_t i = 0; i < cos_section.size(); i++)
	{
		const auto &cos = cos_section[i];

		// Schematas are mandatory
		if (!cos["schemata"])
			throw std::runtime_error("Each cos must have an schemata");
		auto mask = cos["schemata"].as<uint64_t>();

		// CPUs are not mandatory, but note that COS 0 will have all the CPUs by defect
		auto cpus = vector<uint32_t>();
		if (cos["cpus"])
		{
			auto cpulist = cos["cpus"];
			for (auto it = cpulist.begin(); it != cpulist.end(); it++)
			{
				int cpu = it->as<int>();
				cpus.push_back(cpu);
			}
		}

		result.push_back(Cos(mask, cpus));
	}

	return result;
}


vector<Task> config_read_tasks(const YAML::Node &config)
{
	YAML::Node tasks = config["tasks"];
	auto result = vector<Task>();
	for (size_t i = 0; i < tasks.size(); i++)
	{
		if (!tasks[i]["app"])
			throw std::runtime_error("Each task must have an app dictionary with at leask the key 'cmd', and optionally the keys 'stdout', 'stdin', 'stderr', 'skel' and 'max_megainstr'");

		const auto &app = tasks[i]["app"];

		// Commandline
		if (!app["cmd"])
			throw std::runtime_error("Each task must have a cmd");
		string cmd = app["cmd"].as<string>();

		// Dir containing files to copy to rundir
		string skel = app["skel"] ? app["skel"].as<string>() : "";

		// Stdin/out/err redirection
		string output = app["stdout"] ? app["stdout"].as<string>() : "out";
		string input = app["stdin"] ? app["stdin"].as<string>() : "";
		string error = app["stderr"] ? app["stderr"].as<string>() : "err";

		// Instructions limit (in millions of instructions)
		auto max_megainstr = app["max_megainstr"] ? app["max_megainstr"].as<uint64_t>() : -1ULL;

		// CPUS
		auto cpus = vector<int>(); // Default, no affinity
		if (tasks[i]["cpus"])
		{
			auto cpulist = tasks[i]["cpus"];
			for (auto it = cpulist.begin(); it != cpulist.end(); it++)
			{
				int cpu = it->as<int>();
				cpus.push_back(cpu);
			}
		}

		// Max megainstructions override... this is for overcoming the lack of merge operand in the YAML library, don't blame me
		max_megainstr = tasks[i]["max_megainstr"] ? tasks[i]["max_megainstr"].as<uint64_t>() : max_megainstr;

		result.push_back(Task(cmd, cpus, output, input, error, skel, max_megainstr * 1000 * 1000));
	}
	return result;
}


void dir_copy(const string source, const string dest)
{
	if (!fs::exists(source) || !fs::is_directory(source))
		throw std::runtime_error("Source directory " + source + " does not exist or is not a directory");
	if (fs::exists(dest))
		throw std::runtime_error("Destination directory " + dest + " already exists");
	if (!fs::create_directories(dest))
		throw std::runtime_error("Cannot create destination directory " + dest);

	typedef fs::recursive_directory_iterator RDIter;
	for (auto it = RDIter(source), end = RDIter(); it != end; ++it)
	{
		const auto &path = it->path();
		auto relpath = it->path().string();
		boost::replace_first(relpath, source, ""); // Convert the path to a relative path

		fs::copy(path, dest + "/" + relpath);
	}
}


void tasks_set_rundirs(vector<Task> &tasklist, const string &rundir_base)
{
	for (size_t i = 0; i < tasklist.size(); i++)
	{
		auto &task = tasklist[i];
		task.rundir = rundir_base + "/" + to_string(i) + "-" + task.executable;
		if (fs::exists(task.rundir))
			throw std::runtime_error("The rundir '" + task.rundir + "' already exists");
	}
}


void task_create_rundir(const Task &task)
{
	// Create rundir, either empty or with the files from the skel dir
	if (task.skel != "")
		dir_copy(task.skel, task.rundir);
	else
		if (!fs::create_directories(task.rundir))
			throw std::runtime_error("Could not create rundir directory " + task.rundir);

}


void task_remove_rundir(const Task &task)
{
	fs::remove_all(task.rundir);
}


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


// Pause task
void task_pause(const Task &task)
{
	int pid = task.pid;
	int status;

	if (pid <= 1)
		throw std::runtime_error("Tried to send SIGSTOP to pid " + to_string(pid) + ", check for bugs");

	kill(pid, SIGSTOP); // Stop child process
	waitpid(pid, &status, WUNTRACED); // Wait until it stops
	if (WIFEXITED(status))
		throw std::runtime_error("Command '" + task.cmd + "' with pid " + to_string(pid) + " exited unexpectedly with status " + to_string(WEXITSTATUS(status)));
}


// Pause multiple tasks
void tasks_pause(const vector<Task> &tasklist)
{
	for (const auto &task : tasklist)
		kill(task.pid, SIGSTOP); // Stop process

	for (const auto &task : tasklist)
	{
		int pid = task.pid;
		int status;

		if (pid <= 1)
			throw std::runtime_error("Tried to send SIGSTOP to pid " + to_string(pid) + ", check for bugs");

		waitpid(pid, &status, WUNTRACED); // Ensure it stopt
		if (WIFEXITED(status))
			throw std::runtime_error("Command '" + task.cmd + "' with pid " + to_string(pid) + " exited unexpectedly with status " + to_string(WEXITSTATUS(status)));
	}
}


// Resume multiple tasks
void tasks_resume(const vector<Task> &tasklist)
{
	for (const auto &task : tasklist)
		kill(task.pid, SIGCONT); // Resume process

	for (const auto &task : tasklist)
	{
		int pid = task.pid;
		int status;

		if (pid <= 1)
			throw std::runtime_error("Tried to send SIGCONT to pid " + to_string(pid) + ", check for bugs");

		waitpid(pid, &status, WCONTINUED); // Ensure it resumed
		if (WIFEXITED(status))
			throw std::runtime_error("Command '" + task.cmd + "' with pid " + to_string(pid) + " exited unexpectedly with status " + to_string(WEXITSTATUS(status)));
	}
}


// Drop sudo privileges
void drop_privileges()
{
	const char *uidstr = getenv("SUDO_UID");
	const char *gidstr = getenv("SUDO_GID");
	const char *userstr = getenv("SUDO_USER");

	if (!uidstr || !gidstr || !userstr)
		return;

	const uid_t uid = std::stol(uidstr);
	const gid_t gid = std::stol(gidstr);

	if (uid == getuid() && gid == getgid())
		return;

	if (setgid(gid) < 0)
		throw std::runtime_error("Cannot change gid: " + string(strerror(errno)));

	if (initgroups(userstr, gid) < 0)
		throw std::runtime_error("Cannot change group access list: " + string(strerror(errno)));

	if (setuid(uid) < 0)
		throw std::runtime_error("Cannot change uid: " + string(strerror(errno)));
}


// Execute a task and immediately pause it
void task_execute(Task &task)
{
	int argc;
	char **argv;

	if (!g_shell_parse_argv(task.cmd.c_str(), &argc, &argv, NULL))
		throw std::runtime_error("Could not parse commandline '" + task.cmd + "'");

	pid_t pid = fork();
	switch (pid) {
		// Child
		case 0:
		{
			// Set CPU affinity
			cpu_set_t mask;
			CPU_ZERO(&mask);
			for (size_t i = 0; i < task.cpus.size(); i++)
				CPU_SET(task.cpus[i], &mask);
			if (sched_setaffinity(0, sizeof(mask), &mask) < 0)
			{
				cerr << "Could not set CPU affinity: " << strerror(errno) << endl;
				exit(EXIT_FAILURE);
			}

			// Drop sudo privileges
			try
			{
				drop_privileges();
			}
			catch (const std::exception &e)
			{
				cerr << "Failed to drop privileges: " + string(e.what()) << endl;
			}

			// Create rundir with the necessary files and cd into it
			try
			{
				task_create_rundir(task);
			}
			catch (const std::exception &e)
			{
				cerr << "Could not create rundir: " + string(e.what()) << endl;
				exit(EXIT_FAILURE);
			}
			fs::current_path(task.rundir);

			// Redirect OUT/IN/ERR
			if (task.in != "")
			{
				fclose(stdin);
				if (fopen(task.in.c_str(), "r") == NULL)
				{
					cerr << "Failed to start program '" + task.cmd + "', could not open " + task.in << endl;
					exit(EXIT_FAILURE);
				}
			}
			if (task.out != "")
			{
				fclose(stdout);
				if (fopen(task.out.c_str(), "w") == NULL)
				{
					cerr << "Failed to start program '" + task.cmd + "', could not open " + task.out << endl;
					exit(EXIT_FAILURE);
				}
			}
			if (task.err != "")
			{
				fclose(stderr);
				if (fopen(task.err.c_str(), "w") == NULL)
				{
					cerr << "Failed to start program '" + task.cmd + "', could not open " + task.err << endl;
					exit(EXIT_FAILURE);
				}
			}

			// Exec
			execvp(argv[0], argv);

			// Should not reach this
			cerr << "Failed to start program '" + task.cmd + "'" << endl;
			exit(EXIT_FAILURE);
		}

			// Error
		case -1:
			throw std::runtime_error("Failed to start program '" + task.cmd + "'");

			// Parent
		default:
			usleep(100); // Wait a bit, just in case
			task.pid = pid;
			task_pause(task);
			g_strfreev(argv); // Free the memory allocated for argv
			break;
	}
}


void task_kill(Task &task)
{
	int pid = task.pid;
	if (pid > 1) // Never send kill to PID 0 or 1...
	{
		if (kill(pid, SIGKILL) < 0)
			throw std::runtime_error("Could not SIGKILL command '" + task.cmd + "' with pid " + to_string(pid) + ": " + strerror(errno));
		waitpid(pid, NULL, 0); // Wait until it exits...
		task.pid = 0;
	}
	else
	{
		throw std::runtime_error("Tried to kill pid " + to_string(pid) + ", check for bugs");
	}
}


void task_kill_and_restart(Task &task)
{
	task_kill(task);
	task_remove_rundir(task);
	task_execute(task);
}


// Measure the time the passed callable object consumes
template<typename TimeT = chr::milliseconds>
struct measure
{
	template<typename F, typename ...Args>
		static typename TimeT::rep execution(F func, Args&&... args)
		{
			auto start = chr::system_clock::now();

			// Now call the function with all the parameters you need.
			func(std::forward<Args>(args)...);

			auto duration = chr::duration_cast<TimeT>
				(chr::system_clock::now() - start);

			return duration.count();
		}
};


vector<uint32_t> tasks_cores_used(const vector<Task> &tasklist)
{
	auto res = vector<uint32_t>();
	for (const auto &task : tasklist)
		res.push_back(task.cpus[0]);
	//TODO: Ensure no more than one CPU per task and that there are no repeated CPUs
	return res;
}


void tasks_restart(vector<Task> &tasklist)
{
	for (auto &task : tasklist)
	{
		if (task.limit_reached)
		{
			task_kill_and_restart(task);
			task.reset();
		}
	}
}


void stats_print(const Stats &stats, std::ostream &out, uint32_t cpu, uint32_t id, const string &app, uint64_t interval = -1ULL)
{
	boost::io::ios_all_saver guard(out); // Saves current flags and format

	if (interval != -1ULL)
		out << interval << ", ";

	out << cpu << ", ";
	out << std::setfill('0') << std::setw(2) << id << "_" << app << ", ";
	stats.print(out);
}



void loop(vector<Task> &tasklist, vector<Cos> &coslist, CAT &cat, double time_int, double time_max, std::ostream &out, std::ostream &fin_out)
{
	if (time_int <= 0)
		throw std::runtime_error("Interval time must be positive and greater than 0");
	if (time_max <= 0)
		throw std::runtime_error("Max time must be positive and greater than 0");

	// For adjusting the time sleeping
	const int delay_us       = int(time_int * 1000 * 1000);
	const double kp          = 0.5;
	const int initial_offset = 17000; // Empirically determined
	int adj_delay_us         = delay_us - initial_offset;

	// Time / intervals passed
	uint64_t interval = 0;
	double time_elap = 0;

	// Print headers
	out     << "interval, cpu, app_id, ipc, instr, cycles, ms, ev0, ev1, ev2, ev3, ev4" << endl;
	fin_out <<           "cpu, app_id, ipc, instr, cycles, ms, ev0, ev1, ev2, ev3, ev4" << endl;

	// Loop
	while (time_elap < time_max)
	{
		auto cores             = tasks_cores_used(tasklist);
		bool all_completed     = true; // Have all the tasks reached their execution limit?

		// Run for some time and pause
		tasks_resume(tasklist);
		pcm_before();
		sleep_for(chr::microseconds(adj_delay_us));
		auto stats = pcm_after(cores);
		tasks_pause(tasklist);

		// Adjust time...
		adj_delay_us = adj_delay_us + kp * (delay_us - (int) stats[0].ms * 1000);

		// Process tasks...
		for (size_t i = 0; i < tasklist.size(); i++)
		{
			auto &task = tasklist[i];

			// Count stats
			task.stats_interval    = stats[i];
			task.stats_acumulated += stats[i];

			// Instruction limit reached
			if (task.stats_acumulated.instructions >= task.max_instr)
			{
				task.limit_reached = true;

				// It's the first time it reaches the limit
				if (!task.completed)
				{
					task.completed = true;

					// Print interval stats for the last time
					stats_print(task.stats_interval, out, task.cpus[0], task.id, task.executable, interval);

					// Print acumulated stats
					stats_print(task.stats_acumulated, fin_out, task.cpus[0], task.id, task.executable);
				}
			}

			// This task has never reached any limits
			if (!task.completed)
			{
				all_completed = false;
				stats_print(task.stats_interval, out, task.cpus[0], task.id, task.executable, interval);
			}
		}

		// All the tasks have reached their limit -> finish execution
		if (all_completed)
			break;

		// Restart the ones that have reached their limit
		else
			tasks_restart(tasklist);

		// Update time and interval
		interval++;
		time_elap += stats[0].ms / 1000.0;
	}
}


// Leave the machine in a consistent state
void clean(vector<Task> &tasklist, CAT &cat)
{
	cat.cleanup();
	pcm_clean();

	// Try to drop privileges before killing anything
	drop_privileges();

	for (auto &task : tasklist)
		task_kill(task);
}


void clean_and_die(vector<Task> &tasklist, CAT &cat)
{
	cerr << "--- PANIC, TRYING TO CLEAN ---" << endl;

	try
	{
		if (cat.is_initialized())
			cat.reset();
	}
	catch (const std::exception &e)
	{
		cerr << "Could not reset CAT: " << e.what() << endl;
	}

	try
	{
		pcm_clean();
	}
	catch (const std::exception &e)
	{
		cerr << "Could not clean PCM: " << e.what() << endl;
	}

	// Try to drop privileges before killing anything
	drop_privileges();

	for (auto &task : tasklist)
	{
		try
		{
			if (task.pid > 0)
				task_kill(task);
		}
		catch (const std::exception &e)
		{
			cerr << e.what() << endl;
		}
	}

	exit(EXIT_FAILURE);
}


void config_read(const string &path, vector<Task> &tasklist, vector<Cos> &coslist)
{
	YAML::Node config = YAML::LoadFile(path);

	// Setup COS
	if (config["cos"])
		coslist = config_read_cos(config);

	// Read tasks into objects
	if (config["tasks"])
		tasklist = config_read_tasks(config);

	// Check that all COS (but 0) have cpus or tasks assigned
	for (size_t i = 1; i < coslist.size(); i++)
	{
		const auto &cos = coslist[i];
		if (cos.cpus.empty())
			cerr << "Warning: COS " + to_string(i) + " has no assigned CPUs" << endl;
	}
}


std::string random_string(size_t length)
{
	auto randchar = []() -> char
	{
		const char charset[] =
			"0123456789"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz";
		const size_t max_index = (sizeof(charset) - 1);
		return charset[ rand() % max_index ];
	};
	std::string str(length,0);
	std::generate_n( str.begin(), length, randchar );
	return str;
}


int main(int argc, char *argv[])
{
	srand(time(NULL));

	po::options_description desc("Allowed options");
	desc.add_options()
		("help,h", "print usage message")
		("config,c", po::value<string>()->required(), "pathname for yaml config file")
		("output,o", po::value<string>(), "pathname for output")
		("fin-output", po::value<string>()->default_value("/dev/null"), "pathname for output values when tasks are completed")
		("rundir", po::value<string>()->default_value("run"), "directory for creating the directories where the applications are gonna be executed")
		("id", po::value<string>()->default_value(random_string(5)), "identifier for the experiment")
		("ti", po::value<double>()->default_value(1), "time-int, duration in seconds of the time interval to sample performance counters.")
		("tm", po::value<double>()->default_value(std::numeric_limits<double>::max()), "time-max, maximum execution time in seconds, where execution time is computed adding all the intervals executed.")
		("event,e", po::value<vector<string>>()->composing()->multitoken()->required(), "optional list of custom events to monitor (up to 4)")
		("reset-cat", po::value<bool>()->default_value(true), "reset CAT config, before and after")
		// ("cores,c", po::value<vector<int>>()->composing()->multitoken(), "enable specific cores to output")
		;

	bool option_error = false;
	po::variables_map vm;
	try
	{
		// Parse the options without storing them in a map.
		po::parsed_options parsed_options = po::command_line_parser(argc, argv)
			.options(desc)
			.run();

		po::store(parsed_options, vm);
		po::notify(vm);
	}
	catch(const std::exception &e)
	{
		cerr << "Error: " << e.what() << "\n\n";
		option_error = true;
	}

	if (vm.count("help") || option_error)
	{
		cout << desc << endl;
		exit(EXIT_SUCCESS);
	}

	// Open output file if needed; if not, use cout
	auto file = std::ofstream();
	if (vm.count("output"))
		file = open_ofstream(vm["output"].as<string>());
	std::ostream &out = file.is_open() ? file : cout;

	// Output file for final output
	auto fin_out = open_ofstream(vm["fin-output"].as<string>());

	// Read config
	auto tasklist = vector<Task>();
	auto coslist = vector<Cos>();
	string config_file;
	try
	{
		// Read config and set tasklist and coslist
		config_file = vm["config"].as<string>();
		config_read(config_file, tasklist, coslist);
		tasks_set_rundirs(tasklist, vm["rundir"].as<string>() + "/" + vm["id"].as<string>());
	}
	catch(const YAML::ParserException &e)
	{
		cerr << string("Error in config file in line: ") + to_string(e.mark.line) + " col: " + to_string(e.mark.column) + " pos: " + to_string(e.mark.pos) + ": " + e.msg << endl;
		exit(EXIT_FAILURE);
	}
	catch(const std::exception &e)
	{
		cerr << "Error in config file '" + config_file + "': " << e.what() << endl;
		exit(EXIT_FAILURE);
	}

	auto cat = CAT();
	try
	{
		// Configure PCM
		pcm_setup(vm["event"].as<vector<string>>());

		// Configure CAT
		cat = cat_setup(coslist, vm["reset-cat"].as<bool>());

		// Execute and immediately pause tasks
		for (auto &task : tasklist)
			task_execute(task);

		// Start doing things
		loop(tasklist, coslist, cat, vm["ti"].as<double>(), vm["tm"].as<double>(), out, fin_out);

		// Kill tasks, reset CAT, performance monitors, etc...
		clean(tasklist, cat);
	}
	catch(const std::exception &e)
	{
		cerr << "Error: " << e.what() << endl;
		clean_and_die(tasklist, cat);
	}
}
