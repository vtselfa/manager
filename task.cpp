#include <iomanip>
#include <iostream>
#include <sstream>

#include <sched.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <boost/filesystem.hpp>
#include <glib.h>

#include "log.hpp"
#include "task.hpp"
#include "throw-with-trace.hpp"


namespace acc = boost::accumulators;
namespace fs = boost::filesystem;

using std::to_string;
using std::string;
using std::cerr;
using std::endl;


// Init static atribute
std::atomic<uint32_t> Task::ID(0);


void tasks_set_rundirs(std::vector<Task> &tasklist, const std::string &rundir_base)
{
	for (size_t i = 0; i < tasklist.size(); i++)
	{
		auto &task = tasklist[i];
		task.rundir = rundir_base + "/" + std::to_string(i) + "-" + task.executable;
		if (fs::exists(task.rundir))
			throw_with_trace(std::runtime_error("The rundir '" + task.rundir + "' already exists"));
	}
}


void task_create_rundir(const Task &task)
{
	// Create rundir, either empty or with the files from the skel dir
	if (task.skel != "")
		dir_copy(task.skel, task.rundir);
	else
		if (!fs::create_directories(task.rundir))
			throw_with_trace(std::runtime_error("Could not create rundir directory " + task.rundir));

}


void task_remove_rundir(const Task &task)
{
	fs::remove_all(task.rundir);
}


// Pause task
void task_pause(const Task &task)
{
	int pid = task.pid;
	int status;

	if (pid <= 1)
		throw_with_trace(std::runtime_error("Tried to send SIGSTOP to pid " + to_string(pid) + ", check for bugs"));

	kill(pid, SIGSTOP); // Stop child process
	waitpid(pid, &status, WUNTRACED); // Wait until it stops
	if (WIFEXITED(status))
		throw_with_trace(std::runtime_error("Command '" + task.cmd + "' with pid " + to_string(pid) + " exited unexpectedly with status " + to_string(WEXITSTATUS(status))));
}


// Pause multiple tasks
void tasks_pause(const std::vector<Task> &tasklist)
{
	for (const auto &task : tasklist)
		kill(task.pid, SIGSTOP); // Stop process

	for (const auto &task : tasklist)
	{
		int pid = task.pid;
		int status;

		if (pid <= 1)
			throw_with_trace(std::runtime_error("Tried to send SIGSTOP to pid " + to_string(pid) + ", check for bugs"));

		waitpid(pid, &status, WUNTRACED); // Ensure it stopt
		if (WIFEXITED(status))
			throw_with_trace(std::runtime_error("Command '" + task.cmd + "' with pid " + to_string(pid) + " exited unexpectedly with status " + to_string(WEXITSTATUS(status))));
	}
}


// Resume multiple tasks
void tasks_resume(const std::vector<Task> &tasklist)
{
	for (const auto &task : tasklist)
		kill(task.pid, SIGCONT); // Resume process

	for (const auto &task : tasklist)
	{
		int pid = task.pid;
		int status;

		if (pid <= 1)
			throw_with_trace(std::runtime_error("Tried to send SIGCONT to pid " + to_string(pid) + ", check for bugs"));

		waitpid(pid, &status, WCONTINUED); // Ensure it resumed
		if (WIFEXITED(status))
			throw_with_trace(std::runtime_error("Command '" + task.cmd + "' with pid " + to_string(pid) + " exited unexpectedly with status " + to_string(WEXITSTATUS(status))));
	}
}


// Execute a task and immediately pause it
void task_execute(Task &task)
{
	int argc;
	char **argv;

	if (!g_shell_parse_argv(task.cmd.c_str(), &argc, &argv, NULL))
		throw_with_trace(std::runtime_error("Could not parse commandline '" + task.cmd + "'"));

	pid_t pid = fork();
	switch (pid) {
		// Child
		case 0:
		{
			// Set CPU affinity
			try
			{
				set_cpu_affinity({task.cpu});
			}
			catch (const std::exception &e)
			{
				cerr << "Error executing '" + task.cmd + "': " + e.what() << endl;
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
			throw_with_trace(std::runtime_error("Failed to start program '" + task.cmd + "'"));

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
			throw_with_trace(std::runtime_error("Could not SIGKILL command '" + task.cmd + "' with pid " + to_string(pid) + ": " + strerror(errno)));
		waitpid(pid, NULL, 0); // Wait until it exits...
		task.pid = 0;
	}
	else
	{
		throw_with_trace(std::runtime_error("Tried to kill pid " + to_string(pid) + ", check for bugs"));
	}
}


// Kill and restart a task
void task_kill_and_restart(Task &task)
{
	task_kill(task);
	task.reset();
	task_remove_rundir(task);
	task_execute(task);
}


std::vector<uint32_t> tasks_cores_used(const std::vector<Task> &tasklist)
{
	auto res = std::vector<uint32_t>();
	for (const auto &task : tasklist)
		res.push_back(task.cpu);
	return res;
}


// Kill and restart the tasks that have reached their exec limit
void tasks_kill_and_restart(std::vector<Task> &tasklist)
{
	for (auto &task : tasklist)
		if (task.limit_reached)
			task_kill_and_restart(task);
}


void task_stats_print(const Task &t, StatsKind sk, uint64_t interval, std::ostream &out, const std::string &sep)
{
	Stats s;
	if (sk == StatsKind::interval)
		s = t.stats_interval;
	else if (sk == StatsKind::until_compl_summary)
		s = t.stats_acumulated;
	else if (sk == StatsKind::total_summary)
		s = t.stats_total;
	else
		LOGFAT("Unknown stats kind");

	out << interval           << sep;
	out << t.cpu              << sep << std::setfill('0') << std::setw(2);
	out << t.id << "_" << t.executable << sep;

	if (sk == StatsKind::interval || sk == StatsKind::total_summary)
		out << (t.max_instr ? (double) t.stats_total.instructions / (double) t.max_instr : 0) << sep;

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
	out << std::endl;
}


void task_stats_print_headers(const Task &t, StatsKind sk, std::ostream &out, const std::string &sep)
{
	out << "interval" << sep;
	out << "core" << sep;
	out << "app" << sep;

	if (sk == StatsKind::interval || sk == StatsKind::total_summary)
		out << "compl" << sep;

	out << "us" << sep;
	out << "instructions" << sep;
	out << "cycles" << sep;
	out << "invariant_cycles" << sep;
	out << "ipc" << sep;
	out << "ipnc" << sep;
	out << "rel_freq" << sep;
	out << "act_rel_freq" << sep;
	out << "l3_kbytes_occ" << sep;
	out << "mc_gbytes_rd" << sep;
	out << "mc_gbytes_wt" << sep;
	out << "proc_energy" << sep;
	out << "dram_energy";

	for (const auto &kv : t.stats_total.events) // It should not matter which stats we iterate...
	{
		out << sep;
		out << kv.first;
	}

	out << std::endl;
}
