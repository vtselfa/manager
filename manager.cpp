#include <iostream>
#include <chrono>
#include <thread>
#include <cstring>

#include <grp.h>
#include <sched.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <boost/dynamic_bitset.hpp>
#include <boost/program_options.hpp>
#include <glib.h>
#include <yaml-cpp/yaml.h>

#include "cat.hpp"
#include "common.hpp"
#include "manager_pcm.hpp"


namespace po = boost::program_options;
namespace chr = std::chrono;

using std::string;
using std::to_string;
using std::vector;
using std::this_thread::sleep_for;
using boost::dynamic_bitset;
using std::cout;
using std::cerr;
using std::endl;


struct Cos
{
	dynamic_bitset<> schemata; // Ways assigned mask
	dynamic_bitset<> cpus;     // CPUs affected mask
	vector<int> tasks;         // Indexes of tasks affected (this is NOT a list of PIDs)

	Cos(dynamic_bitset<> schemata,	dynamic_bitset<> cpus, vector<int> tasks = {}) : schemata(schemata), cpus(cpus), tasks(tasks) {}
};


struct Task
{
	string cmd;
	vector<int> cpus; // Allowed cpus
	int cos;          // Assigned COS, 0 -> default COS
	int pid;          // Set after executing the task

	Task(string cmd, vector<int> cpus, int cos = 0, int pid = 0) : cmd(cmd), cpus(cpus), cos(cos), pid(pid) {}
	Task(string cmd, int cos, int pid = 0) : cmd(cmd), cos(cos), pid(pid) {}
};


vector<Cos> config_cos(const YAML::Node &config)
{
	YAML::Node cos_section = config["cos"];
	auto result = vector<Cos>();

	if (cos_section.Type() != YAML::NodeType::Sequence)
		throw std::runtime_error("In the config file, the cos section must contain a sequence");

	for (size_t i = 0; i < cos_section.size(); i++)
	{
		dynamic_bitset<> sche(MAX_WAYS, 0); // Invalid on purpose
		dynamic_bitset<> cpus(MAX_CPUS, 0); // Default, no core assigned
		const auto &cos = cos_section[i];

		// Schematas are mandatory
		if (!cos["schemata"])
			throw std::runtime_error("Each cos must have an schemata");
		sche = dynamic_bitset<>(MAX_WAYS, cos["schemata"].as<unsigned int>());

		// CPUs are not mandatory, but then it will be checked that there is at least one task assigned to the COS
		if (cos["cpus"])
		{
			if (i == 0)
				throw std::runtime_error("Default COS cannot have a CPU mask, it's defined by the other COS");
			cpus = dynamic_bitset<>(MAX_CPUS, cos["cpus"].as<unsigned int>());
		}

		result.push_back(Cos(sche, cpus));
	}

	// TODO: Fix default COS cpus
	return result;
}


vector<Task> config_tasks(const YAML::Node &config)
{
	YAML::Node tasks = config["tasks"];
	auto result = vector<Task>();
	for (size_t i = 0; i < tasks.size(); i++)
	{
		// Commandline
		if (!tasks[i]["cmd"])
			throw std::runtime_error("Each task must have a cmd");
		string cmd = tasks[i]["cmd"].as<string>();

		// CPUS
		vector<int> cpus; // Default, no affinity
		if (tasks[i]["cpus"])
		{
			auto cpulist = tasks[i]["cpus"];
			for (auto it = cpulist.begin(); it != cpulist.end(); it++)
			{
				int cpu = it->as<int>();
				cpus.push_back(cpu);
			}
		}

		// COS
		unsigned int cos = 0;
		if (tasks[i]["cos"])
		{
			cos = tasks[i]["cos"].as<unsigned int>();
			if (!config["cos"] && cos != 0) // COS 0 is the default COS, so it's not necesary to define it
				throw std::runtime_error("To assign a COS to a task it has to be defined first");
			if (cos > config["cos"].size())
				throw std::runtime_error("COS number " + std::to_string(cos) + "doesn't exist");
		}

		result.push_back(Task(cmd, cpus, cos));
	}
	return result;
}


void cat_setup(const vector<Cos> &coslist, const vector<Task> &tasklist)
{
	// Not even a default COS defined, we asume the user doesn't want to mess with CAT or has preconfigured it
	if (coslist.empty())
		return;

	// Reset CAT, just in case
	cat_reset();

	// Set default COS first.
	// Note that CPUS cannot be unassigned (i.e. changed from 1 -> 0),
	// only reassigned (assigned to another COS), so if one wants to remove CPUs
	// from default COS they have to be assigned to another COS. Thus, it is
	// incorrect to try to set a CPU mask here. The effective CPU mask it's gonna depend
	// on the other COS settings.
	const auto &defcos = coslist[0];
	cos_set_schemata(".", defcos.schemata);

	// Rest of COS
	for (size_t i = 1; i < coslist.size(); i++)
	{
		const auto &cos = coslist[i];
		auto pids = vector<string>();
		for (int id : cos.tasks)
		{
			const auto &task = tasklist[id];
			if (task.pid > 0)
				pids.push_back(std::to_string(task.pid));
			else
				throw std::runtime_error("Task " + to_string(id) + " is not running");
		}
		cos_create(std::to_string(i), cos.schemata, cos.cpus, pids);
	}
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

	if (!uidstr || !gidstr || userstr)
		return;

	const uid_t uid = std::stol(uidstr);
	const gid_t gid = std::stol(gidstr);

	if (setuid(uid) < 0)
		throw std::runtime_error("Cannot change uid: " + string(strerror(errno)));

	if (setgid(gid) < 0)
		throw std::runtime_error("Cannot change gid: " + string(strerror(errno)));

	if (initgroups(userstr, gid) < 0)
		throw std::runtime_error("Cannot change group access list: " + string(strerror(errno)));
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
			cpu_set_t mask;
			CPU_ZERO(&mask);
			for (size_t i = 0; i < task.cpus.size(); i++)
					CPU_SET(task.cpus[i], &mask);
			sched_setaffinity(0, sizeof(mask), &mask);
			drop_privileges(); // Drop privileges
			execvp(argv[0], argv);
			throw std::runtime_error("Failed to start program '" + task.cmd + "'");

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
		task.pid = 0;
	}
	else
	{
		throw std::runtime_error("Tried to kill pid " + to_string(pid) + ", check for bugs");
	}
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


void loop(vector<Task> tasklist, vector<Cos> coslist, double time_int, double time_max, std::ostream &out)
{
	if (time_int <= 0)
		throw std::runtime_error("Interval time must be positive and greater than 0");
	if (time_max <= 0)
		throw std::runtime_error("Max time must be positive and greater than 0");

	int delay_ms = int(time_int * 1000);
	for (double time_elap = 0; time_elap < time_max; time_elap += time_int)
	{
		// Run for some time and pause
		tasks_resume(tasklist);
		pcm_before();
		sleep_for(chr::milliseconds(delay_ms));
		pcm_after(out);
		tasks_pause(tasklist);
	}
}


// Leave the machine in a consistent state
void clean(vector<Task> &tasklist)
{
	for (auto &task : tasklist)
		task_kill(task);
	cat_reset();
	pcm_clean();
}


void clean_and_die(vector<Task> &tasklist)
{
	cerr << "--- PANIC, TRYING TO CLEAN ---" << endl;

	for (auto &task : tasklist)
	{
		try
		{
			task_kill(task);
		}
		catch (const std::exception &e)
		{
			cerr << "Could not kill task " << task.cmd << "with pid " << task.pid << ": " << e.what() << endl;
		}
	}

	try
	{
		cat_reset();
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

	exit(EXIT_FAILURE);
}


void config_read(const string &path, vector<Task> &tasklist, vector<Cos> &coslist)
{
	YAML::Node config = YAML::LoadFile(path);

	// Setup COS
	if (config["cos"])
		coslist = config_cos(config);

	// Read tasks into objects
	if (config["tasks"])
		tasklist = config_tasks(config);

	// Link tasks and COS
	for (size_t i = 0; i < tasklist.size(); i++)
	{
		int cos = tasklist[i].cos;
		if (cos != 0)
			coslist[cos].tasks.push_back(i);
	}

	// Check that all COS (but 0) have cpus or tasks assigned
	// for (size_t i = 1; i < coslist.size(); i++)
	// {
	// 	const auto &cos = coslist[i];
	// 	if (cos.cpus.none() && cos.tasks.size() == 0)
	// 		throw std::runtime_error("COS " + to_string(i) + " has no assigned CPU nor task");
	// }
}


int main(int argc, char *argv[])
{
	po::options_description desc("Allowed options");
	desc.add_options()
		("help,h", "print usage message")
		("config,c", po::value<string>()->required(), "pathname for yaml config file")
		("output,o", po::value<string>(), "pathname for output")
		("ti", po::value<double>()->default_value(1), "time-int, duration in seconds of the time interval to sample performance counters.")
		("tm", po::value<double>()->default_value(std::numeric_limits<double>::max()), "time-max, maximum execution time in seconds, where execution time is computed adding all the intervals executed.")
		("event,e", po::value<vector<string>>()->composing()->multitoken()->required(), "optional list of custom events to monitor (up to 4)")
		// ("cores,c", po::value<vector<int>>()->composing()->multitoken(), "enable specific cores to output")
		// ("run,r", po::value<string>(), "file with lines like: <coreaffinity> <command>")
		// ("max-intervals", po::value<size_t>()->default_value(numeric_limits<size_t>::max()), "stop after this number of intervals.")
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
		cout << "Error: " << e.what() << "\n\n";
		option_error = true;
	}

	if (vm.count("help") || option_error)
	{
		cout << desc << endl;
		exit(EXIT_SUCCESS);
	}

	// Open output file if needed; if not, use cout
	std::ofstream file;
	if (vm.count("output"))
		file = open_ofstream(vm["output"].as<string>());
	std::ostream &out = file ? file : cout;

	// Read config
	auto tasklist = vector<Task>();
	auto coslist = vector<Cos>();
	string config_file;
	try
	{
		// Read config and set tasklist and coslist
		config_file = vm["config"].as<string>();
		config_read(config_file, tasklist, coslist);
	}
	catch(const std::exception &e)
	{
		cerr << "Error in config file '" + config_file + "': " << e.what() << endl;
		exit(EXIT_FAILURE);
	}

	try
	{
		// Configure PCM
		pcm_setup(vm["event"].as<vector<string>>());

		// Execute and immediately pause tasks
		for (auto &task : tasklist)
			task_execute(task);

		// Configure CAT
		cat_setup(coslist, tasklist);

		// Start doing things
		loop(tasklist, coslist, vm["ti"].as<double>(), vm["tm"].as<double>(), out);

		// Kill tasks, reset CAT, performance monitors, etc...
		clean(tasklist);
	}
	catch(const std::exception &e)
	{
		cerr << "ERROR: " << e.what() << endl;
		clean_and_die(tasklist);
	}
}
