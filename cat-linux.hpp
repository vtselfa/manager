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

#include <cstdint>
#include <map>

#include <boost/filesystem.hpp>

#include "cat.hpp"


class CATInfo
{
	public:

	CATInfo() = default;
	CATInfo(const std::string &_cache, uint64_t _cbm_mask, uint32_t _min_cbm_bits, uint32_t _num_closids) :
			cache(_cache), cbm_mask(_cbm_mask), min_cbm_bits(_min_cbm_bits), num_closids(_num_closids) {}

	std::string cache;
	uint64_t cbm_mask;
	uint32_t min_cbm_bits;
	uint32_t num_closids;
};


class CATLinux : public CAT
{
	protected:

	CATInfo info;

	#define FS boost::filesystem
	void set_schemata(FS::path clos_dir, uint64_t mask);
	void set_cpus(FS::path clos_dir, uint64_t cpu_mask);
	void add_task(FS::path clos_dir, pid_t pid);
	void remove_task(std::string task);

	void create_clos(std::string clos);
	void delete_clos(FS::path clos_dir);
	void delete_all_clos();
	void create_all_clos();

	uint64_t get_schemata(FS::path clos_dir) const;
	uint64_t get_cpus(FS::path clos_dir) const;
	FS::path get_clos_dir(uint32_t cpu) const;
	std::vector<std::string> get_tasks(FS::path clos_dir) const;
	FS::path intel_to_linux(uint32_t clos) const;
	std::vector<FS::path> get_clos_dirs() const;
	const CATInfo& get_info() const { return info; };
	#undef FS

	public:

	CATLinux() = default;

	/* CAT API */
	void init() override;
	void reset() override;

	void set_cbm(uint32_t clos, uint64_t cbm) override;
	void add_cpu(uint32_t clos, uint32_t cpu) override;

	uint32_t get_clos(uint32_t cpu) const override;
	uint64_t get_cbm(uint32_t clos) const override;
	uint32_t get_max_closids() const override;

	void print() override {};

	/* CAT Linux API */
	void add_task(uint32_t clos, pid_t pid);
	void add_tasks(uint32_t clos, const std::vector<pid_t> &pids);

	uint32_t get_clos_of_task(pid_t pid) const;
};


std::map<std::string, CATInfo> cat_read_info();

