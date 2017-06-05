#include <iostream>

#include <fmt/format.h>


#include "events-libpfm.hpp"
#include "throw-with-trace.hpp"


using fmt::literals::operator""_format;


void PFM::init()
{
	if (pfm_initialize() != PFM_SUCCESS)
		throw_with_trace(std::runtime_error("libpfm initialization failed"));
	initialized = true;
}


void PFM::clean()
{
	if (initialized)
	{
		for (auto &item : pid_events)
			perf_free_fds(item.second.fds, item.second.num_fds);
		pfm_terminate();
		initialized = false;
	}
}


void PFM::setup_events(pid_t pid, const std::vector<std::string> &groups)
{
	perf_event_desc_t *fds = NULL;
	int num_fds = 0;

	for (const auto &grp : groups)
	{
		std::cout << grp << std::endl;
		int ret = perf_setup_list_events(grp.c_str(), &fds, &num_fds);
		if (ret || !num_fds)
			throw_with_trace(std::runtime_error("Could not setup events"));
	}

	for (int i = 0; i < num_fds; i++)
	{
		bool is_group_leader;
		int group_fd;

		is_group_leader = perf_is_group_leader(fds, i);
		if (is_group_leader)
			group_fd = -1;
		else
			group_fd = fds[fds[i].group_leader].fd;

		fds[i].hw.read_format = PERF_FORMAT_SCALE;

		// Inherit across forks
		fds[i].hw.inherit = 1;

		fds[i].fd = perf_event_open(&fds[i].hw, pid, -1, group_fd, 0);
		if (fds[i].fd == -1)
			throw_with_trace(std::runtime_error("cannot attach event {} {}"_format(i, fds[i].name)));
	}

	pid_events[pid] = EventDesc(fds, num_fds);
}


void PFM::read_groups(pid_t pid)
{
	auto fds = pid_events[pid].fds;
	auto num_fds = pid_events[pid].num_fds;
	for (int evt = 0; evt < num_fds; evt++)
	{
		uint64_t values[3];
		ssize_t ret = read(fds[evt].fd, values, sizeof(values));
		if (ret != (ssize_t) sizeof(values))
		{
			if (ret == -1)
				throw_with_trace(std::runtime_error("Cannot read event {}"_format(fds[evt].name)));

			/* likely pinned and could not be loaded */
			throw_with_trace(std::runtime_error("Could not read event {}, tried to read {} bytes, but got {}"_format(
					evt, sizeof(values), ret)));
		}

		fds[evt].values[0] = values[0];
		fds[evt].values[1] = values[1];
		fds[evt].values[2] = values[2];
	}
}


void PFM::print_counts(pid_t pid)
{
	read_groups(pid);

	auto fds = pid_events[pid].fds;
	auto num_fds = pid_events[pid].num_fds;

	for(int i = 0; i < num_fds; i++)
	{
		uint64_t val   = perf_scale(fds[i].values);
		uint64_t delta = perf_scale_delta(fds[i].values, fds[i].prev_values);
		double ratio = perf_scale_ratio(fds[i].values);

		/* separate groups */
		if (perf_is_group_leader(fds, i))
			putchar('\n');

		std::cout << "{} {} {} ({}% scaling, ena={}, run={})\n"_format(
			val,
			delta,
			fds[i].name,
			(1.0-ratio)*100.0,
			fds[i].values[1],
			fds[i].values[2]);

		fds[i].prev_values[0] = fds[i].values[0];
		fds[i].prev_values[1] = fds[i].values[1];
		fds[i].prev_values[2] = fds[i].values[2];
	}
}
