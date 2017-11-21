#include <algorithm>
#include <random>
#include <sstream>

#include <fmt/format.h>

#include "cat-linux-policy.hpp"
#include "log.hpp"
#include "sched.hpp"


namespace acc = boost::accumulators;
using fmt::literals::operator""_format;


namespace sched
{

cpu_set_t array_to_cpu_set_t(const std::vector<uint32_t> &cpus);
std::vector<uint32_t> allowed_cpus();
std::vector<uint32_t> allowed_cpus(pid_t pid);


cpu_set_t array_to_cpu_set_t(const std::vector<uint32_t> &cpus)
{
	cpu_set_t mask;
	CPU_ZERO(&mask);
	for (auto cpu : cpus)
		CPU_SET(cpu, &mask);
	return mask;
}


std::vector<uint32_t> allowed_cpus() {return allowed_cpus(getpid());}
std::vector<uint32_t> allowed_cpus(pid_t pid)
{
	cpu_set_t mask;
	if (sched_getaffinity(pid, sizeof(mask), &mask) < 0)
		throw_with_trace(std::runtime_error("Could not get CPU affinity for pid '{}': {}"_format(
				pid, strerror(errno))));

	std::vector<uint32_t> result;

	for (int i = 0; i < CPU_SETSIZE; i++)
	{
		if (CPU_ISSET(i, &mask))
			result.push_back(i);
	}
	return result;
}


void Fair::outliers(const tasklist_t &tasklist, tasklist_t &upper, tasklist_t &lower)
{
	acc::accumulator_set<double, acc::stats<acc::tag::mean, acc::tag::variance>> accum;

	for(const auto &task : tasklist)
	{
		double value = acc::rolling_mean(stall_time.at(task->id));
		accum(value);
	}

	double uth = acc::mean(accum) + 2 * std::sqrt(acc::variance(accum));
	double lth = acc::mean(accum) - 2 * std::sqrt(acc::variance(accum));
	upper.clear();
	lower.clear();
	for(const auto &task : tasklist)
	{
		double value = acc::rolling_mean(stall_time.at(task->id));
		if (value > uth)
			upper.push_back(task);
		if (value < lth)
			lower.push_back(task);
	}
}


tasklist_t Fair::apply(const tasklist_t &tasklist)
{
	LOGDEB("Fair scheduling");

	size_t max_num_tasks = cpus.size();
	assert(!cpus.empty());

	double cycles = 0;
	if (!sched_last.empty())
	{
		for (const auto &task : tasklist)
		{
			if (sched_last.at(task->id))
			{
				cycles = task->stats.last("cycles");
				break;
			}
		}
	}
	assert(cycles || sched_last.empty());

	for (const auto &task : tasklist)
	{
		// First exec of the task
		if (sched_last.find(task->id) == sched_last.end())
		{
			sched_last.emplace(task->id, false);
			stall_time.emplace(task->id, accum_t(acc::tag::rolling_window::window_size = 5));
		}

		// Every other exec
		else
		{
			// If was not scheduled, then count the interval cycles as stalled
			if (!sched_last.at(task->id))
				stall_time.at(task->id)(cycles);
			// If was scheduled then accumulate the stall cycles
			else
				stall_time.at(task->id)(task->stats.last("cycle_activity.stalls_ldm_pending"));
		}
	}

	// Detect outliers
	tasklist_t upper, lower;
	outliers(tasklist, upper, lower);
	LOGDEB(iterable_to_string(upper.begin(), upper.end(), [](const auto &t) {return "{}:{}"_format(t->id, t->name);}, " "));
	LOGDEB(iterable_to_string(upper.begin(), upper.end(), [](const auto &t) {return "{}:{}"_format(t->id, t->name);}, " "));

	auto clustering = cat::policy::Cluster_KMeans(4, cat_read_info()["L3"].num_closids, str_to_evalclusters("dunn"), "cycle_activity.stalls_ldm_pending", false);
	auto clusters = clustering.apply(tasklist);

	for (size_t i = 0; i < clusters.size(); i++)
	{
		std::string task_ids;
		const auto &points = clusters[i].getPoints();
		size_t p = 0;
		for (const auto &point : points)
		{
			const auto &task = tasks_find(tasklist, point->id);
			task_ids += "{}:{}"_format(task->id, task->name);
			task_ids += (p == points.size() - 1) ? "" : ", ";
			p++;
		}
		LOGDEB(fmt::format("{{Cluster {}: {{tasks: [{}]}}}}", i, task_ids));
	}

	std::vector<uint32_t> table;
	auto weights = std::vector<int>{1, 2, 4, 8};
	for (size_t i = 0; i < clusters.size(); i++)
	{
		for (const auto &point : clusters[i].getPoints())
		{
			const auto &task = tasks_find(tasklist, point->id);
			for (int j = 0; j < weights[i]; j++)
				table.push_back(task->id);
		}
	}

	// Decide tasks to run
	tasklist_t result;
	std::default_random_engine generator;
	for (size_t i = 0; i < std::min(max_num_tasks, tasklist.size()); i++)
	{
		std::uniform_int_distribution<int> distribution(0, table.size() - 1);
		uint32_t pos = distribution(generator);
		uint32_t id = table[pos];
		const auto &task = tasks_find(tasklist, id);

		// Delete from table
		table.erase(std::remove_if(table.begin(), table.end(), [id](auto v){return v == id;}), table.end());

		result.push_back(task);
	}

	assert(result.size() == max_num_tasks || tasklist.size() < max_num_tasks);
	Base::set_cpu_affinity(result);

	// Store who has been scheduled
	for (const auto &task : tasklist)
		sched_last.at(task->id) = false;
	for (const auto &task : result)
		sched_last.at(task->id) = true;

	return result;
}


void Base::set_cpu_affinity(const tasklist_t &tasklist) const
{
	for (const auto &task : tasklist)
	{
		cpu_set_t task_mask = array_to_cpu_set_t(task->cpus);
		cpu_set_t sched_mask = array_to_cpu_set_t(this->cpus);
		CPU_AND(&task_mask, &task_mask, &sched_mask);

		if (CPU_COUNT(&task_mask) == 0)
			throw_with_trace(std::runtime_error("CPU affinity mask for task {}:{} is empty"_format(task->id, task->name)));

		if (sched_setaffinity(task->pid, sizeof(task_mask), &task_mask) < 0)
			throw_with_trace(std::runtime_error("Could not set CPU affinity for task {}:{}: {}"_format(
					task->id, task->name, strerror(errno))));
	}
}


tasklist_t Base::apply(const tasklist_t &tasklist)
{
	LOGDEB("Linux scheduling");
	set_cpu_affinity(tasklist);
	return tasklist;
}


tasklist_t Random::apply(const tasklist_t &tasklist)
{
	LOGDEB("Random scheduling");

	size_t max_num_tasks = cpus.size();
	assert(!cpus.empty());

	tasklist_t result(tasklist);

	std::random_shuffle(result.begin(), result.end());

	int surplus = tasklist.size() - max_num_tasks;
	while(surplus > 0)
	{
		result.pop_back();
		surplus--;
	}

	assert(result.size() == max_num_tasks || tasklist.size() < max_num_tasks);

	set_cpu_affinity(result);

	return result;
}


const std::string Base::show(const tasklist_t &tasklist) const
{
	if (tasklist.empty()) return "Tasks scheduled: []";

	std::stringstream ss;
	ss << "Tasks scheduled: [";
	size_t i;
	for (i = 0; i < tasklist.size() - 1; i++)
		ss << "{}:{}, "_format(tasklist[i]->id, tasklist[i]->name);
	ss << "{}:{}]"_format(tasklist[i]->id, tasklist[i]->name);

	return ss.str();
}


Status::Status(pid_t _pid)
{
	std::ifstream status;
	status.open("/proc/{}/status"_format(_pid));

	std::string name;
	std::string value;

	while (status)
	{
		std::getline(status, name, ':');
		status >> std::ws;
		std::getline(status, value);
		d.emplace(name, value);
	}
}


Stat::Stat(pid_t _pid)
{
	auto stat = open_ifstream("/proc/{}/stat"_format(_pid));

	// (1) The process ID.
	stat >> pid;
	// (2) The filename of the executable, in parentheses.
	stat >> comm;
	// (3) One of the following characters, indicating process state:
	stat >> state;
	//  (4) The PID of the parent of this process.
	stat >> ppid;
	// (5) The process group ID of the process.
	stat >> pgrp;
	// (6) The session ID of the process.
	stat >> session;
	// (7) The controlling terminal of the process.
	stat >> tty_nr;
	// (8) The ID of the foreground process group of the controlling terminal of the process.
	stat >> tpgid;
	// (9) The kernel flags word of the process.
	stat >> flags;
	// (10) The number of minor faults the process has made which have not required loading a memory page from disk.
	stat >> minflt;
	// (11) The number of minor faults that the process's waited for children have made.
	stat >> cminflt;
	// (12) The number of major faults the process has made which have required loading a memory page from disk.
	stat >> majflt;
	// (13) The number of major faults that the process's waited-for children have made.
	stat >> cmajflt;
	// (14) Amount of time that this process has been scheduled in user mode, measured in clock ticks (divide by sysconf(_SC_CLK_TCK)).
	stat >> utime;
	// (15) Amount of time that this process has been scheduled in kernel mode, measured in clock ticks (divide by sysconf(_SC_CLK_TCK)).
	stat >> stime;
	// (16) Amount of time that this process's waited for children have been scheduled in user mode, measured in clock ticks (divide by sysconf(_SC_CLK_TCK)).
	stat >> cutime; //  %ld
	// (17) Amount of time that this process's waited for children have been scheduled in kernel mode, measured in clock ticks (divide by sysconf(_SC_CLK_TCK)).
	stat >> cstime;
	// (18) Raw scheduling priority value.
	stat >> priority;
	// (19) The nice value (see setpriority(2)), a value in the range 19 (low priority) to -20 (high priority).
	stat >> nice;
	// (20) Number of threads in this process.
	stat >> num_threads;
	// (21) Not used, 0.
	stat >> itrealvalue;
	// (22) The time the process started after system boot in clock ticks (divide by sysconf(_SC_CLK_TCK)).
	stat >> starttime;
	// (23) Virtual memory size in bytes.
	stat >> vsize;
	// (24) Resident Set Size: number of pages the process has in real memory.
	stat >> rss;
	// (25) Current soft limit in bytes on the rss of the process.
	stat >> rsslim;
	// (26) The address above which program text can run.
	stat >> startcode;
	// (27) The address below which program text can run.
	stat >> endcode;
	// (28) The address of the start (i.e., bottom) of the stack.
	stat >> startstack;
	// (29) The current value of ESP (stack pointer), as found in the kernel stack page for the process.
	stat >> kstkesp;
	// (30) The current EIP (instruction pointer).
	stat >> kstkeip;
	// (31) The bitmap of pending signals, displayed as a decimal number. Obsolete, use /proc/[pid]/status instead.
	stat >> signal;
	// (32) The bitmap of blocked signals, displayed as a decimal number. Obsolete, use /proc/[pid]/status instead.
	stat >> blocked;
	// (33) The bitmap of ignored signals, displayed as a decimal number. Obsolete, use /proc/[pid]/status instead.
	stat >> sigignore;
	// (34) The bitmap of caught signals, displayed as a decimal number. Obsolete, use /proc/[pid]/status instead.
	stat >> sigcatch;
	// (35) This is the "channel" in which the process is waiting.
	stat >> wchan;
	// (36) Number of pages swapped (not maintained).
	stat >> nswap;
	// (37) Cumulative nswap for child processes (not maintained).
	stat >> cnswap;
	// (38) Signal to be sent to parent when we die.
	stat >> exit_signal;
	// (39) CPU number last executed on.
	stat >> processor;
	// (40) Real-time scheduling priority.
	stat >> rt_priority;
	// (41) Scheduling policy.
	stat >> policy;
	// (42) Aggregated block I/O delays, measured in clock ticks (centiseconds).
	stat >> delayacct_blkio_ticks;
	// (43) Guest time of the process (time spent running a virtual CPU for a guest operating system), measured in clock ticks (divide by sysconf(_SC_CLK_TCK)).
	stat >> guest_time;
	// (44) Guest time of the process's children, measured in clock ticks (divide by sysconf(_SC_CLK_TCK)).
	stat >> cguest_time;
	// (45) Address above which program initialized and uninitialized (BSS) data are placed.
	stat >> start_data;
	// (46) Address below which program initialized and uninitialized (BSS) data are placed.
	stat >> end_data;
	// (47) Address above which program heap can be expanded with brk(2).
	stat >> start_brk;
	// (48) Address above which program command-line arguments (argv) are placed.
	stat >> arg_start;
	// (49) Address below program command-line arguments (argv) are placed.
	stat >> arg_end;
	// (50) Address above which program environment is placed.
	stat >> env_start;
	// (51) Address below which program environment is placed.
	stat >> env_end;
	// (52) The thread's exit status in the form reported by waitpid(2).
	stat >> exit_code;

	if (stat && stat.peek() != '\n')
		throw_with_trace(std::runtime_error("There are camps in /proc/{}/stat that have not been read"_format(_pid)));
}

} // namespace sched
