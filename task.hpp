#pragma once

#include <atomic>
#include <vector>

#include "cat-linux.hpp"
#include "common.hpp"
#include "stats.hpp"


class Task
{
	// Number of tasks created
	static std::atomic<uint32_t> ID;

	public:

	enum class Status
	{
		runnable,
		limit_reached,
		exited,
		done,
	};

	// Set on construction
	const uint32_t id;
	const std::string name;
	const std::string cmd;
	const uint32_t initial_clos;   // The CLOS this app starts mapped to
	const std::string out;         // Stdout redirection
	const std::string in;          // Stdin redirection
	const std::string err;         // Stderr redirection
	const std::string skel;        // Directory containing files and folders to copy to rundir
	const uint64_t max_instr;      // Max number of instructions to execute
	const uint32_t max_restarts;   // Maximum number of times this application is gonna be restarted after reaching the instruction limit or finishing
	const bool batch;              // Batch tasks do not need to be completed in order to finish the execution

	std::vector<uint32_t> cpus;    // Allowed cpus
	std::string rundir = ""; // Set before executing the task
	pid_t pid = 0;           // Set after executing the task

	Stats stats = Stats();

	uint32_t num_restarts = 0;  // Number of times it has reached the instruction limit
	uint32_t completed = 0;     // Number of times it has reached the instruction limit

	Task() = delete;
	Task(const std::string &name, const std::string &cmd, uint32_t initial_clos,
			const std::vector<uint32_t> &cpus, const std::string &out, const std::string &in,
			const std::string &err, const std::string &skel, uint64_t max_instr,
			uint32_t max_restarts, bool batch) :
		id(ID++), name(name), cmd(cmd),
		initial_clos(initial_clos),
		out(out), in(in), err(err), skel(skel),
		max_instr(max_instr), max_restarts(max_restarts),
		batch(batch), cpus(cpus) {}

	static
	const std::string status_to_str(const Status& s);

	const std::string status_to_str() const;
	const Status& get_status() const;
	void set_status(const Status &new_status);
	void reset();

	private:

	Status status = Status::runnable;
};
typedef std::vector<std::shared_ptr<Task>> tasklist_t;


void tasks_set_rundirs(tasklist_t &tasklist, const std::string &rundir_base);
void tasks_pause(tasklist_t &tasklist);
void tasks_resume(const tasklist_t &tasklist);
void tasks_kill_and_restart(tasklist_t &tasklist, Perf &perf, const std::vector<std::string> &events);
void tasks_map_to_initial_clos(tasklist_t &tasklist, const std::shared_ptr<CATLinux> &cat);
std::vector<uint32_t> tasks_cores_used(const tasklist_t &tasklist);

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
