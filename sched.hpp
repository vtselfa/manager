#pragma once

#include <libcpuid.h>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>

#include "task.hpp"
#include "throw-with-trace.hpp"


namespace sched
{

std::vector<uint32_t> allowed_cpus();
std::vector<uint32_t> allowed_cpus(pid_t pid);


class CpuId
{
	bool initialized = false;
	struct cpu_id_t data;

	public:

	CpuId() = default;
	const struct cpu_id_t& operator()();
};
extern CpuId cpu_id;


class Base
{
	protected:

	// Allowed cpus
	const std::vector<uint32_t> cpus;

	// ¬XOR of the cpus allowed for the task and the scheduler
	void set_cpu_affinity(const tasklist_t &tasklist) const;

	public:

	Base() = default;
	Base(const std::vector<uint32_t> &_cpus) : cpus(_cpus) {};
	~Base() = default;

	const std::string show(const tasklist_t &tasklist) const;
	virtual tasklist_t apply(const tasklist_t &tasklist);
};
typedef std::shared_ptr<Base> ptr_t;


// Just do as the base class
class Linux : public Base
{
	public:

	Linux() = default;
	Linux(const std::vector<uint32_t> &_cpus) : Base(_cpus) {};
	~Linux() = default;
};


class Fair : public Base
{
	// Declare the 'accum_t' typedef
	#define ACC boost::accumulators
	typedef ACC::accumulator_set <
		double,
		ACC::stats <
			ACC::tag::sum,
			ACC::tag::rolling_mean>> accum_t;
	#undef ACC
	std::map<uint32_t, accum_t> stall_time;
	std::map<uint32_t, bool> sched_last;

	void outliers(const tasklist_t &tasklist, tasklist_t &upper, tasklist_t &lower);

	public:

	Fair() = default;
	Fair(const std::vector<uint32_t> &_cpus) : Base(_cpus) {};
	~Fair() = default;

	virtual tasklist_t apply(const tasklist_t &tasklist) override;
};


class Random : public Base
{
	public:

	Random() = default;
	Random(const std::vector<uint32_t> &_cpus) : Base(_cpus) {};
	~Random() = default;

	virtual tasklist_t apply(const tasklist_t &tasklist) override;
};


class Status
{
	std::map<std::string, std::string> d;

	public:

	Status() = delete;
	Status(pid_t _pid);
	const std::string operator()(const std::string &name) const { return d.at(name); }
};


class Stat
{
	public:

	Stat() = delete;
	Stat(pid_t _pid);

	// (1) The process ID.
	int pid;
	// (2) The filename of the executable, in parentheses.
	std::string comm;
	// (3) One of the following characters, indicating process state:
	char state;
	// (4) The PID of the parent of this process.
	int ppid;
	// (5) The process group ID of the process.
	int pgrp;
	// (6) The session ID of the process.
	int session;
	// (7) The controlling terminal of the process.
	int tty_nr;
	// (8) The ID of the foreground process group of the controlling terminal of the process.
	int tpgid;
	// (9) The kernel flags word of the process.
	unsigned int flags;
	// (10) The number of minor faults the process has made which have not required loading a memory page from disk.
	unsigned long minflt;
	// (11) The number of minor faults that the process's waited for children have made.
	unsigned long cminflt;
	// (12) The number of major faults the process has made which have required loading a memory page from disk.
	unsigned long majflt;
	// (13) The number of major faults that the process's waited-for children have made.
	unsigned long cmajflt;
	// (14) Amount of time that this process has been scheduled in user mode, measured in clock ticks (divide by sysconf(_SC_CLK_TCK)).
	unsigned long utime;
	// (15) Amount of time that this process has been scheduled in kernel mode, measured in clock ticks (divide by sysconf(_SC_CLK_TCK)).
	unsigned long stime;
	// (16) Amount of time that this process's waited for children have been scheduled in user mode, measured in clock ticks (divide by sysconf(_SC_CLK_TCK)).
	long cutime; //  %ld
	// (17) Amount of time that this process's waited for children have been scheduled in kernel mode, measured in clock ticks (divide by sysconf(_SC_CLK_TCK)).
	long cstime;
	// (18) Raw scheduling priority value.
	long priority;
	// (19) The nice value (see setpriority(2)), a value in the range 19 (low priority) to -20 (high priority).
	long nice;
	// (20) Number of threads in this process.
	long num_threads;
	// (21) Not used, 0.
	long itrealvalue;
	// (22) The time the process started after system boot in clock ticks (divide by sysconf(_SC_CLK_TCK)).
	long long starttime;
	// (23) Virtual memory size in bytes.
	unsigned long vsize;
	// (24) Resident Set Size: number of pages the process has in real memory.
	long rss;
	// (25) Current soft limit in bytes on the rss of the process.
	unsigned long rsslim;
	// (26) The address above which program text can run.
	unsigned long startcode;
	// (27) The address below which program text can run.
	unsigned long endcode;
	// (28) The address of the start (i.e., bottom) of the stack.
	unsigned long startstack;
	// (29) The current value of ESP (stack pointer), as found in the kernel stack page for the process.
	unsigned long kstkesp;
	// (30) The current EIP (instruction pointer).
	unsigned long kstkeip;
	// (31) The bitmap of pending signals, displayed as a decimal number. Obsolete, use /proc/[pid]/status instead.
	unsigned long signal;
	// (32) The bitmap of blocked signals, displayed as a decimal number. Obsolete, use /proc/[pid]/status instead.
	unsigned long blocked;
	// (33) The bitmap of ignored signals, displayed as a decimal number. Obsolete, use /proc/[pid]/status instead.
	unsigned long sigignore;
	// (34) The bitmap of caught signals, displayed as a decimal number. Obsolete, use /proc/[pid]/status instead.
	unsigned long sigcatch;
	// (35) This is the "channel" in which the process is waiting.
	unsigned long wchan;
	// (36) Number of pages swapped (not maintained).
	unsigned long nswap;
	// (37) Cumulative nswap for child processes (not maintained).
	unsigned long cnswap;
	// (38) Signal to be sent to parent when we die.
	int exit_signal;
	// (39) CPU number last executed on.
	int processor;
	// (40) Real-time scheduling priority.
	unsigned int rt_priority;
	// (41) Scheduling policy.
	unsigned int policy;
	// (42) Aggregated block I/O delays, measured in clock ticks (centiseconds).
	unsigned long long delayacct_blkio_ticks;
	// (43) Guest time of the process (time spent running a virtual CPU for a guest operating system), measured in clock ticks (divide by sysconf(_SC_CLK_TCK)).
	unsigned long guest_time;
	// (44) Guest time of the process's children, measured in clock ticks (divide by sysconf(_SC_CLK_TCK)).
	long cguest_time;
	// (45) Address above which program initialized and uninitialized (BSS) data are placed.
	unsigned long start_data;
	// (46) Address below which program initialized and uninitialized (BSS) data are placed.
	unsigned long end_data;
	// (47) Address above which program heap can be expanded with brk(2).
	unsigned long start_brk;
	// (48) Address above which program command-line arguments (argv) are placed.
	unsigned long arg_start;
	// (49) Address below program command-line arguments (argv) are placed.
	unsigned long arg_end;
	// (50) Address above which program environment is placed.
	unsigned long env_start;
	// (51) Address below which program environment is placed.
	unsigned long env_end;
	// (52) The thread's exit status in the form reported by waitpid(2).
	int exit_code;
};


} // namespace sched
