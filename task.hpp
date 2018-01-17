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
	Task(const std::string &_name, const std::string &_cmd, uint32_t _initial_clos,
			const std::vector<uint32_t> &_cpus, const std::string &_out, const std::string &_in,
			const std::string &_err, const std::string &_skel, uint64_t _max_instr,
			uint32_t _max_restarts, bool _batch) :
		id(ID++), name(_name), cmd(_cmd),
		initial_clos(_initial_clos),
		out(_out), in(_in), err(_err), skel(_skel),
		max_instr(_max_instr), max_restarts(_max_restarts),
		batch(_batch), cpus(_cpus) {}

	static
	const std::string status_to_str(const Status& s);

	const std::string status_to_str() const;
	const Status& get_status() const;
	void set_status(const Status &new_status);
	void reset();

	private:

	Status status = Status::runnable;
};
typedef std::shared_ptr<Task> task_ptr_t;
typedef std::vector<task_ptr_t> tasklist_t;


void tasks_set_rundirs(tasklist_t &tasklist, const std::string &rundir_base);
void tasks_pause(tasklist_t &tasklist);
void tasks_resume(const tasklist_t &tasklist);
void tasks_map_to_initial_clos(tasklist_t &tasklist, const std::shared_ptr<CATLinux> &cat);
std::vector<uint32_t> tasks_cores_used(const tasklist_t &tasklist);
const task_ptr_t& tasks_find(const tasklist_t &tasklist, uint32_t id);

void task_create_rundir(const Task &task);
void task_remove_rundir(const Task &task);

void task_execute(Task &task);
void task_pause(const Task &task);
void task_resume(const Task &task);
void task_kill(Task &task);
void task_restart(Task &task);
void task_kill_and_restart(Task &task);
bool task_exited(const Task &task); // Test if the task has exited

// If the limit has been reached, kill the application.
// If the limit of restarts has not been reached, restart the application. If the limit of restarts was reached, mark the application as done.
void task_restart_or_set_done(Task &task, Perf &perf, const std::vector<std::string> &events);

void task_stats_print_headers(const Task &t, std::ostream &out, const std::string &sep = ",");
void task_stats_print_interval(const Task &t, uint64_t interval, std::ostream &out, const std::string &sep = ",");
void task_stats_print_total(const Task &t, uint64_t interval, std::ostream &out, const std::string &sep = ",");
