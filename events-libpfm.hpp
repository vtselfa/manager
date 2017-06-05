#pragma once


#include <map>
#include <vector>
extern "C"
{
#include "perf_util.h"
}


class PFM
{
	struct EventDesc
	{
		perf_event_desc_t *fds;
		int num_fds;
		EventDesc() = default;
		EventDesc(perf_event_desc_t *fds, int num_fds) :
				fds(fds), num_fds(num_fds) {};
	};

	std::map<pid_t, EventDesc> pid_events;
	bool initialized = false;

	public:

	PFM() = default;

	// Allow move members
	PFM(PFM&&) = default;
	PFM& operator=(PFM&&) = default;

	// Delete copy members
	PFM(const PFM&) = delete;
	PFM& operator=(const PFM&) = delete;

	~PFM() { clean(); }


	void init();
	void clean();
	void setup_events(pid_t pid, const std::vector<std::string> &groups);
	void read_groups(pid_t pid);
	void print_counts(pid_t pid);
};
