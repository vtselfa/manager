#pragma once


#include <map>
#include <vector>

struct perf_evlist;

class Perf
{
	struct EventDesc
	{
		std::vector<struct perf_evlist*> groups;

		EventDesc() = default;
		EventDesc(const std::vector<struct perf_evlist*> &groups) :
				groups(groups) {};
		void append(struct perf_evlist *ev_list) { groups.push_back(ev_list); }
	};

	std::map<pid_t, EventDesc> pid_events;
	bool initialized = false;

	public:

	Perf() = default;

	// Allow move members
	Perf(Perf&&) = default;
	Perf& operator=(Perf&&) = default;

	// Delete copy members
	Perf(const Perf&) = delete;
	Perf& operator=(const Perf&) = delete;

	~Perf() { clean(); }


	void init();
	void clean();
	void setup_events(pid_t pid, const std::vector<std::string> &groups);
	void read_groups(pid_t pid);
	void print_counts(pid_t pid);
};
