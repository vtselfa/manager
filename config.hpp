#pragma once

#include <cstdint>
#include <memory>
#include <vector>


class CAT_Policy;
class Task;;


struct Cos
{
	uint64_t mask;              // Ways assigned mask
	std::vector<uint32_t> cpus; // Associated CPUs

	Cos(uint64_t mask, std::vector<uint32_t> cpus = {}) : mask(mask), cpus(cpus){}
};


void config_read(const std::string &path, std::vector<Task> &tasklist, std::vector<Cos> &coslist, std::shared_ptr<CAT_Policy> &catpol);
