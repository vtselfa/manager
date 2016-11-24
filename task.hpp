#pragma once

#include <atomic>
#include <vector>

#include "common.hpp"
#include "stats.hpp"


struct Task
{
	// Number of tasks created
	static std::atomic<uint32_t> ID;

	// Set on construction
	const uint32_t id;
	const std::string cmd;
	const std::string executable;  // Basename of the executable
	uint32_t cpu;             // Allowed cpus
	const std::string out;         // Stdout redirection
	const std::string in;          // Stdin redirection
	const std::string err;         // Stderr redirection
	const std::string skel;        // Directory containing files and folders to copy to rundir
	const uint64_t max_instr; // Max number of instructions to execute

	std::string rundir = ""; // Set before executing the task
	int pid       = 0;  // Set after executing the task

	Stats stats_acumulated = Stats(); // From the start of the execution, but reseted each time it arrives to the target goal
	Stats stats_total = Stats();      // From the start of the execution
	Stats stats_interval   = Stats(); // Only last interval

	bool limit_reached = false; // Has the instruction limit been reached?
	bool completed = false;     // Do not print stats
	bool batch = false;         // Batch tasks do not need to be completed in order to finish the execution

	Task() = delete;
	Task(std::string cmd, uint32_t cpu, std::string out, std::string in, std::string err, std::string skel, uint64_t max_instr, bool batch) :
		id(ID++), cmd(cmd), executable(extract_executable_name(cmd)), cpu(cpu), out(out), in(in), err(err), skel(skel), max_instr(max_instr), batch(batch) {}

	// Reset stats and limit flag
	void reset()
	{
		limit_reached = false;
		stats_acumulated = Stats();
		stats_interval   = Stats();
	}
};


void tasks_set_rundirs(std::vector<Task> &tasklist, const std::string &rundir_base);
void task_create_rundir(const Task &task);
void task_remove_rundir(const Task &task);
void task_pause(const Task &task);
void tasks_pause(const std::vector<Task> &tasklist);
void tasks_resume(const std::vector<Task> &tasklist);
void task_execute(Task &task);
void task_kill(Task &task);
void task_kill_and_restart(Task &task);
std::vector<uint32_t> tasks_cores_used(const std::vector<Task> &tasklist);
void tasks_kill_and_restart(std::vector<Task> &tasklist);
