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

	Cos(uint64_t mask, std::vector<uint32_t> cpus = {}) : mask(mask), cpus(cpus){}
};


void config_read(const std::string &path, const std::string &overlay, tasklist_t &tasklist, std::vector<Cos> &coslist, std::shared_ptr<cat::policy::Base> &catpol, sched::ptr_t &sched);
