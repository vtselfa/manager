/*
Copyright (C) 2016  Vicent Selfa (viselol@disca.upv.es)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <boost/dynamic_bitset_fwd.hpp>


class CAT
{
	private:

	uint32_t max_cpus;
	uint32_t max_ways;
	uint32_t max_cos;

	dynamic_bitset<> read_default_schemata();
	uint32_t read_max_num_cpus();
	uint32_t read_max_num_cos();

	void create_cos(uint32_t cos);
	void delete_cos(uint32_t cos);
	bool exists_cos(uint32_t cos);

	public:

	CAT();

	~CAT() = default;

	void set_schemata(uint32_t cos, uint64_t mask);
	void reset_schemata(uint32_t cos);

	void pin_cpu(uint32_t cos, uint32_t core);
	void unpin_cpu(uint32_t core);
	void reset_cpus(uint32_t cos);

	void pin_task(uint32_t cos, uint32_t pid);
	void unpin_task(uint32_t pid);
	void reset_tasks(uint32_t cos);

	void reset();
	void print();
}
