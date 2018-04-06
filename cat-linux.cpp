#include <fstream>
#include <iostream>

#include <fmt/format.h>

#include "cat-linux.hpp"
#include "common.hpp"
#include "throw-with-trace.hpp"


#define ROOT "/sys/fs/resctrl"
#define INFO_DIR ROOT "/info"


namespace fs = boost::filesystem;

using std::string;
using std::vector;
using fmt::literals::operator""_format;


std::map<std::string, CATInfo> cat_read_info()
{
	std::map<string, CATInfo> info;
	for(auto &p: fs::directory_iterator(INFO_DIR))
	{
		string cache = fs::basename(p);
		uint64_t cbm_mask;
		uint32_t min_cbm_bits;
		uint32_t num_closids;

		try
		{
			std::ifstream f;
			f = open_ifstream(p / "cbm_mask");
			f >> std::hex >> cbm_mask;
			f = open_ifstream(p / "min_cbm_bits");
			f >> min_cbm_bits;
			f = open_ifstream(p / "num_closids");
			f >> num_closids;
		}
		catch(const std::system_error &e)
		{
			throw_with_trace(std::runtime_error("Cannot read CAT info '{}': {}"_format(INFO_DIR, strerror(errno))));
		}

		info[cache] = CATInfo(cache, cbm_mask, min_cbm_bits, num_closids);
	}
	return info;
}


void CATLinux::set_schemata(fs::path clos_dir, uint64_t mask)
{
	assert_dir_exists(clos_dir);
	const std::string schemata = "{}:0={:x}"_format(info.cache, mask);

	try
	{
		std::ofstream f = open_ofstream(clos_dir / "schemata");
		f << schemata << std::endl;
	}
	catch(const std::system_error &e)
	{
		throw_with_trace(std::runtime_error("Cannot set schemata of CLOS '{}': {}"_format(clos_dir.string(), strerror(errno))));
	}
}


uint64_t CATLinux::get_schemata(fs::path clos_dir) const
{
	uint64_t schemata;

	assert_dir_exists(clos_dir);
	try
	{
		std::ifstream f = open_ifstream(clos_dir / "schemata");
		f.ignore(std::numeric_limits<std::streamsize>::max(), '=');
		f >> std::hex >> schemata;
	}
	catch(const std::system_error &e)
	{
		throw_with_trace(std::runtime_error("Cannot get schemata of CLOS '{}': {}"_format(clos_dir.string(), strerror(errno))));
	}

	return schemata;
}


void CATLinux::create_clos(std::string clos)
{
	auto path = fs::path(ROOT) / clos;
	if (fs::exists(path))
		throw_with_trace(std::runtime_error("Cannot create CLOS: directory {} already exists"_format(path.string())));

	fs::create_directory(path);
}


void CATLinux::set_cpus(fs::path clos_dir, uint64_t cpu_mask)
{
	assert_dir_exists(clos_dir);
	std::ofstream f = open_ofstream(clos_dir / "cpus");
	f << std::hex << cpu_mask << std::endl;
}


uint64_t CATLinux::get_cpus(fs::path clos_dir) const
{
	uint64_t cpu_mask;
	assert_dir_exists(clos_dir);
	std::ifstream f = open_ifstream(clos_dir / "cpus");
	f >> std::hex >> cpu_mask;
	return cpu_mask;
}


fs::path CATLinux::get_clos_dir(uint32_t cpu) const
{
	uint64_t cpu_mask = 1ULL << cpu;
	if (get_cpus(fs::path(ROOT)) & cpu_mask)
		return fs::path(ROOT);

	for(auto &p: fs::directory_iterator(ROOT))
		if (fs::is_directory(p) && fs::basename(p) != "info")
			if (get_cpus(p) & cpu_mask)
				return p;
	throw_with_trace(std::runtime_error("CPU {} is not in any CLOS, does it exist?"));
}


std::vector<fs::path> CATLinux::get_clos_dirs() const
{
	auto result = std::vector<fs::path>();
	for(auto &p: fs::directory_iterator(ROOT))
		if (fs::is_directory(p) && fs::basename(p) != "info")
			result.push_back(p);
	return result;
}


/* Get the tasks assigned to a COS. */
std::vector<std::string> CATLinux::get_tasks(fs::path clos_dir) const
{
	assert_dir_exists(clos_dir);
	std::ifstream f;
	f.open("{}/tasks"_format(clos_dir.string()));

	vector<string> tasks;
	string task;
	while (f >> task)
		tasks.push_back(task);
	return tasks;
}


void CATLinux::add_task(fs::path clos_dir, pid_t pid)
{
	assert_dir_exists(clos_dir);
	try
	{
		std::ofstream f = open_ofstream(clos_dir / "tasks");
		f << pid << std::endl;
	}
	catch(const std::system_error &e)
	{
		throw_with_trace(std::runtime_error("Cannot write pid '{}' into '{}'"_format(pid, (clos_dir / "tasks").string())));
	}
}


void CATLinux::add_task(uint32_t clos, pid_t pid)
{
	add_task(intel_to_linux(clos), pid);
}


void CATLinux::add_tasks(uint32_t clos, const std::vector<pid_t> &pids)
{
	for (const auto &pid : pids)
		add_task(intel_to_linux(clos), pid);
}


void CATLinux::remove_task(std::string task)
{
	std::ofstream f = open_ofstream(fs::path(ROOT) / "tasks");
	f << task << std::endl;
}


/* Deletes a COS.
 * Will throw if it does not exist of the deletion fails. */
void CATLinux::delete_clos(fs::path clos_dir)
{
	if (!fs::exists(clos_dir))
		throw_with_trace(std::runtime_error("The COS " + clos_dir.string() + " does not exist"));
	fs::remove(clos_dir);
}


// Remove all COS
// Since we are iterating an special filesystem we cannot trust the iterator remaining valid after a deletion
// Therefore, first store targets and then delete them
void CATLinux::delete_all_clos()
{
	auto to_remove = vector<fs::path>();
	for(const auto &p: fs::directory_iterator(ROOT))
		if (fs::is_directory(p) && fs::basename(p) != "info")
			to_remove.push_back(p);
	for(const auto &p: to_remove)
		delete_clos(p);
}


void CATLinux::create_all_clos()
{
	for (uint32_t i = 1; i < get_max_closids(); i++)
		create_clos(std::to_string(i)); // For compatibility with Intel pqos library...
}


fs::path CATLinux::intel_to_linux(uint32_t clos) const
{
	if (clos == 0)
		return fs::path(ROOT);
	else
		return fs::path(ROOT) / std::to_string(clos);
}


void CATLinux::init()
{
	initialized = true;
	auto infomap = cat_read_info();
	info = infomap["L3"];
	reset();
	create_all_clos();
}


void CATLinux::reset()
{
	delete_all_clos();

	create_all_clos();

	// Reset all CBMs (this should be automatic, but it's not)
	for (uint32_t i = 0; i < get_max_closids(); i++)
		set_cbm(i, info.cbm_mask);

	delete_all_clos();
}


void CATLinux::set_cbm(uint32_t clos, uint64_t cbm)
{
	set_schemata(intel_to_linux(clos), cbm);
}


uint64_t CATLinux::get_cbm(uint32_t clos) const
{
	return get_schemata(intel_to_linux(clos));
}


void CATLinux::add_cpu(uint32_t clos, uint32_t cpu)
{
	fs::path clos_dir = intel_to_linux(clos);
	uint64_t cpu_mask = get_cpus(clos_dir);
	cpu_mask |= (1ULL << cpu);
	set_cpus(clos_dir, cpu_mask);
}


uint32_t CATLinux::get_clos(uint32_t cpu) const
{
	auto path = get_clos_dir(cpu);
	if (path == fs::path(ROOT))
		return 0;
	string clos = fs::basename(get_clos_dir(cpu));
	return std::stoi(clos);
}


uint32_t CATLinux::get_max_closids() const
{
	return CATLinux::info.num_closids;
}


uint32_t CATLinux::get_clos_of_task(pid_t pid) const
{
	auto path = fs::path(ROOT);
	auto tasks = get_tasks(path);
	if (std::find(tasks.begin(), tasks.end(), std::to_string(pid)) != tasks.end())
		return 0;

	for (uint32_t i = 1; i < get_max_closids(); i++)
	{
		path = fs::path(ROOT) / std::to_string(i);
		tasks = get_tasks(path);
		if (std::find(tasks.begin(), tasks.end(), std::to_string(pid)) != tasks.end())
			return i;
	}
	throw_with_trace(std::runtime_error("The PID {} is not in any CLOS, does it exist?"_format(pid)));
}
