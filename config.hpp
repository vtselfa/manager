#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "cat-policy.hpp"
#include "task.hpp"
#include "sched.hpp"


struct Cos
{
	uint64_t mask;              // Ways assigned mask
	std::vector<uint32_t> cpus; // Associated CPUs

	Cos(uint64_t _mask, const std::vector<uint32_t> &_cpus = {}) : mask(_mask), cpus(_cpus) {}
};


void config_read(const std::string &path, const std::string &overlay, tasklist_t &tasklist, std::vector<Cos> &coslist, std::shared_ptr<cat::policy::Base> &catpol, sched::ptr_t &sched);
