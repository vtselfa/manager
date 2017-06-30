#pragma once

#include <atomic>
#include <vector>

#include "cat-linux.hpp"
#include "common.hpp"
#include "stats.hpp"


struct Task
{
	// Number of tasks created
	static std::atomic<uint32_t> ID;

	// Set on construction
	const uint32_t id;
	const std::string name;
	const std::string cmd;
	const uint32_t initial_clos;   // The CLOS this app starts mapped to
	std::vector<uint32_t> cpus;    // Allowed cpus
	const std::string out;         // Stdout redirection
	const std::string in;          // Stdin redirection
	const std::string err;         // Stderr redirection
	const std::string skel;        // Directory containing files and folders to copy to rundir
	const uint64_t max_instr = 0;  // Max number of instructions to execute

	std::string rundir = ""; // Set before executing the task
	pid_t pid = 0;           // Set after executing the task

	Stats stats = Stats();

	bool limit_reached = false; // Has the instruction limit been reached?
	bool finished = false;      // Has the application executed completely?

	uint32_t completed = 0;     // Number of times it has reached the instruction limit
	bool batch = false;         // Batch tasks do not need to be completed in order to finish the execution

	Task() = delete;
	Task(const std::string &name, const std::string &cmd, uint32_t initial_clos, const std::vector<uint32_t> &cpus, const std::string &out, const std::string &in, const std::string &err, const std::string &skel, uint64_t max_instr, bool batch) :
		id(ID++), name(name), cmd(cmd), initial_clos(initial_clos), cpus(cpus), out(out), in(in), err(err), skel(skel), max_instr(max_instr), batch(batch) {}

	// Reset flags
	void reset()
	{
		limit_reached = false;
		finished = false;
		stats.reset_counters();
	}
};
typedef std::vector<Task> tasklist_t;


enum class StatsKind
{
	interval,
	until_compl_summary,
	total_summary
};


void tasks_set_rundirs(std::vector<Task> &tasklist, const std::string &rundir_base);
void tasks_pause(std::vector<Task> &tasklist);
void tasks_resume(const std::vector<Task> &tasklist);
void tasks_kill_and_restart(std::vector<Task> &tasklist, Perf &perf, const std::vector<std::string> &events);
void tasks_map_to_initial_clos(std::vector<Task> &tasklist, const std::shared_ptr<CATLinux> &cat);
std::vector<uint32_t> tasks_cores_used(const std::vector<Task> &tasklist);

void task_create_rundir(const Task &task);
void task_remove_rundir(const Task &task);

void task_execute(Task &task);
void task_pause(const Task &task);
void task_resume(const Task &task);
void task_kill(Task &task);
void task_restart(Task &task);
void task_kill_and_restart(Task &task);
bool task_exited(const Task &task); // Test if the task has exited

void task_stats_print_headers(const Task &t, std::ostream &out, const std::string &sep = ",");
void task_stats_print_interval(const Task &t, uint64_t interval, std::ostream &out, const std::string &sep = ",");
void task_stats_print_total(const Task &t, uint64_t interval, std::ostream &out, const std::string &sep = ",");
