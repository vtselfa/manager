#include <fstream>
#include <iostream>
#include <boost/dynamic_bitset.hpp>
#include <boost/filesystem.hpp>
#include "common.hpp"
#include "cat.hpp"


#define ROOT "/sys/fs/resctrl"
#define MAX_COS 4


namespace fs = boost::filesystem;

using std::string;
using std::vector;
using boost::dynamic_bitset;

static
uint32_t CAT::read_default_schemata() const
{
	uint32_t mask;
	std::ifstream f = open_ifstream(ROOT"/info/L3/cbm_mask");
	f >> std::hex >> mask;
	return mask;
}

static
uint32_t CAT::read_max_num_cpus() const
{
	if (!cpuid_present())
		throw std::runtime_errror("Your CPU doesn't support CPUID");

	struct cpu_raw_data_t raw;
	struct cpu_id_t data;

	if (cpuid_get_raw_data(&raw) < 0)
		throw std::runtime_errror("Cannot get the CPUID raw data: " + cpuid_error());

	if (cpu_identify(&raw, &data) < 0)
		throw std::runtime_error("CPU identification failed: " + cpuid_error());

	return data.num_logical_cpus;
}

uint32_t CAT::read_max_num_cos()
{
	uint64_t max_num_cos;
	std::ifstream f = open_ifstream(ROOT"/info/L3/num_closids");
	f >> std::hex >> mask;
	return dynamic_bitset<>(__builtin_popcount(mask), mask);
}

CAT::CAT
{
	def_sch = read_default_schemata();
	max_ways = __builtin_popcount(def_sch);
	max_cpus = read_max_num_cpus();
	max_cos = read_max_num_cos();

	reset();
}

void CAT::set_schemata(uint32_t cos, uint32_t mask)
{
	const char *prefix = "L3:0=";

	string path = ROOT;
	if (cos == 0)
		path = path + "/schemata";
	else
		path = path + "/" + to_string(cos) + "/schemata";

	std::ofstream f = open_ofstream(path);
	f.exceptions(ios_base::badbit | ios_base::failbit);
    f << prefix << std::hex << mask << std::endl;
}

void CAT::reset_schemata(uint32_t cos);
{
	set_schemata(cos, def_sch);
}

uint32_t get_cpus(uint32_t cos)
{
	validate_cos(cos);
	uint32_t cpus;
	string path = ROOT;
	if (cos == 0)
		path = path + "/cpus";
	else
		path = path + "/" + to_string(cos) + "/cpus";
	std::ifstream f = open_ifstream(path);
    f >> std::hex >> mask;
	return mask;
}

void set_cpus(uint32_t cos, uint32_t cpus)
{
	validate_cos(cos);
	string path = ROOT;
	if (cos == 0)
		path = path + "/cpus";
	else
		path = path + "/" + to_string(cos) + "/cpus";
	std::ofstream f = open_ofstream("cpus");
	f.exceptions(ios_base::badbit | ios_base::failbit);
    f << std::hex << cpus << std::endl;
}

void CAT::pin_cpu(uint32_t cos, uint32_t cpu);
{
	uint32_t cpus = get_cpus(cos);
	uint32_t mask = 1 << cpu;
	cpus |= mask;
	set_cpus(cos, cpus);
}

void CAT::unpin_cpu(uint32_t cpu);
{
	uint32_t cpus = get_cpus(cos);
	uint32_t mask = 1 << cpu;
	cpus &= ~mask;
	set_cpus(cos, cpus);

}

void CAT::reset_cpus(uint32_t cos);
{
	set_cpus(cos, 0);
}

vector<string> CAT::get_tasks(uint32_t cos)
{
	string path = ROOT;
	if (cos == 0)
		path = path + "/tasks";
	else
		path = path + "/" + to_string(cos) + "/tasks";

	std::ifstream f = open_ifstream(path);
	vector<string> tasks;
	string task;
	while (f >> task)
		tasks.push_back(task);
	return tasks;
}

void set_tasks(string cos, vector<string> tasks)
{
	string path = root;
	if (cos == 0)
		path = path + "/tasks";
	else
		path = path + "/" + to_string(cos) + "/tasks";

	// assign tasks
	std::ofstream f = open_ofstream(path);
	for (const auto &task : tasks)
		f << task << std::endl;
}

void CAT::pin_task(uint32_t cos, uint32_t pid);
{
	string path = root;
	if (cos == 0)
		path = path + "/tasks";
	else
		path = path + "/" + to_string(cos) + "/tasks";

	// assign tasks
	std::ofstream f = open_ofstream(path, fstream::app);
	f << to_string(pid) << std::endl;

}

void CAT::unpin_task(uint32_t pid);
{
	pin_task(0, pid);
}

void CAT::reset_tasks(uint32_t cos);
{
	set_tasks(cos, {});
}

/* Deletes a COS.
 * Will throw if it does not exist of the deletion fails. */
void remove_cosid(string cos)
{
	string path = string(ROOT) + "/" + cos;
	if (!fs::exists(path))
		throw std::runtime_error("The COS " + cos + " does not exist");

	cos_reset_tasks(cos);

	// Remove
	fs::remove(path);
}

// Remove all COS
// Since we are iterating an special filesystem we cannot trust the iterator remaining valid after a deletion
// Therefore, first store targets and then delete them
void CAT::remove_cosids()
{
	auto to_remove = vector<fs::path>();
	for(const auto &p: fs::directory_iterator(ROOT))
		if (fs::is_directory(p) && fs::basename(p) != "info")
			to_remove.push_back(p);
	for(const auto &p: to_remove)
		cos_delete(fs::basename(p));
}

void CAT::reset();
{

}

void CAT::print();
{

}

void CAT::validate_cos(uint32_t cos)
{
	if (cos < 0 || cos >= max_cos)
		throw std::runtime_error("Invalid cosid");
}






/* Reads a bitset with the assigned CPUs to a COS.
 * Use 0 for the default COS */
dynamic_bitset<> get_cpus(uint32_t cos)
{
	validate_cos(cos);
	unsigned int mask;
	std::ifstream f = open_ifstream(string(ROOT) + "/" + string(cos) + "/cpus");
    f >> std::hex >> mask;
	return dynamic_bitset<>(max_cpus, mask);
}


/* Reads a bitset with the assigned CPUs to a COS.
 * Use . for the default COS */
//TODO: Reset first...
void cos_set_cpus(string cos, dynamic_bitset<> cpus)
{
	int mask = cpus.to_ulong();
	int mask_res;

	assert(cos != ""); // Use "." for the default COS
	fs::current_path(ROOT);
	fs::current_path(cos);

	std::ofstream f = open_ofstream("cpus");
    f << std::hex << mask << std::dec << std::endl;

	mask_res = cos_get_cpus(cos).to_ulong();
	if (mask != mask_res)
		throw std::runtime_error("Could not set mask for cpus for COS " + cos);
}


/* Reads a bitset with the assigned ways to a COS.
 * The minimum number of ways that can be assigned is 2.
 * Only consecutive chunks of ways can be assigned, i.e. ff0ff = bad, ffff0 = good. */
dynamic_bitset<> cos_get_schemata(string cos)
{
	assert(cos != ""); // Use "." for the default COS
	fs::current_path(ROOT);
	fs::current_path(cos);

	unsigned int mask;
	std::ifstream f = open_ifstream("schemata");
	f.ignore(std::numeric_limits<std::streamsize>::max(), '=');
    f >> std::hex >> mask;
	return dynamic_bitset<>(MAX_WAYS, mask);
}


/* Writes a bitset with the assigned ways to a COS.
 * The minimum number of ways that can be assigned is 2.
 * Only consecutive chunks of ways can be assigned, i.e. ff0ff = bad, ffff0 = good. */
void cos_set_schemata(string cos, dynamic_bitset<> schemata)
{
}


/* Creates a new dir for a COS.
 * Each new dir is automatically populated with 3 files, schemata, cpus and tasks. */
void cos_mkdir(string cos)
{
	fs::current_path(ROOT);

	int count = 0;
	for(auto &p: fs::directory_iterator("."))
		if (fs::is_directory(p) && p != "./info")
			count++;
	if (count >= MAX_COS)
		throw std::runtime_error("Too many COS, the maximum is " + std::to_string(MAX_COS));

	if (fs::exists(cos))
		throw std::runtime_error("COS " + cos + " already exists");

	fs::create_directory(cos);
}


/* Get the tasks assigned to a COS. */
vector<string> cos_get_tasks(string cos)
{
	if (cos == ".")
		throw std::runtime_error("There is no point in reading the tasks assigned to the default COS, check for bugs");

	string path = string(ROOT) + "/" + cos + "/tasks";
	std::ifstream f = open_ifstream(path);
	vector<string> tasks;
	string task;
	while (f >> task)
		tasks.push_back(task);
	return tasks;
}


/* Remove all tasks from a COS.
 * For this, the tasks need to be written to the task list of the default COS. */
void cos_reset_tasks(string cos)
{
	vector<string> tasks = cos_get_tasks(cos);
	if (tasks.size() > 0)
		cos_set_tasks(".", tasks);
}


/* Set the tasks assigned to a COS.
 * Previously assigned tasks are removed. */
void cos_set_tasks(string cos, vector<string> tasks)
{
	// Remove previously assigned tasks
	if (cos != ".")
		cos_reset_tasks(cos);

	// Assign tasks
	string path = string(ROOT) + "/" + cos + "/tasks";
	std::ofstream f = open_ofstream(path);
	for (const auto &task : tasks)
		f << task << std::endl;

	// Verify it worked (there is no point in the case of the default COS)
	if (cos != ".")
	{
		vector<string> tasks_res = cos_get_tasks(cos);
		unsigned int i;
		for (i = 0; i < tasks.size() && i < tasks_res.size(); i++)
			if (tasks[i] != tasks_res[i])
				break;
		if (i != tasks.size())
			throw std::runtime_error("At least task " + tasks[i] + " could not be assigned to COS " + cos + ". Check if it exists");
	}
}


/* Creates a new COS.
 * Will throw exceptions if the COS already exists, if there are too many and if for whatever reason cannot create it properly. */
void cos_create(string cos, dynamic_bitset<> schemata, vector<string> tasks)
{
	// Dir
	cos_mkdir(cos);

	// Schemata
	cos_set_schemata(cos, schemata);

	// Tasks
	cos_set_tasks(cos, tasks);
}


/* Creates a new COS.
 * Will throw exceptions if the COS already exists, if there are too many and if for whatever reason cannot create it properly. */
void cos_create(string cos, dynamic_bitset<> schemata, dynamic_bitset<> cpus, vector<string> tasks)
{
	// Dir
	cos_mkdir(cos);

	// Schemata
	cos_set_schemata(cos, schemata);

	// CPUs
	cos_set_cpus(cos, cpus);

	// Tasks
	cos_set_tasks(cos, tasks);
}


/* Deletes a COS.
 * Will throw if it does not exist of the deletion fails. */
void cos_delete(string cos)
{
	string path = string(ROOT) + "/" + cos;
	if (!fs::exists(path))
		throw std::runtime_error("The COS " + cos + " does not exist");

	cos_reset_tasks(cos);

	// Reset schemata
	cos_set_schemata(cos, dynamic_bitset<>(MAX_WAYS, -1ul));

	// Reset cpus
	cos_set_cpus(cos, dynamic_bitset<>(MAX_CPUS, -1ul));

	// Remove
	fs::remove(path);
}


// Remove all COS
// Since we are iterating an special filesystem we cannot trust the iterator remaining valid after a deletion
// Therefore, first store targets and then delete them
void cos_delete_all()
{
	auto to_remove = vector<fs::path>();
	for(const auto &p: fs::directory_iterator(ROOT))
		if (fs::is_directory(p) && fs::basename(p) != "info")
			to_remove.push_back(p);
	for(const auto &p: to_remove)
		cos_delete(fs::basename(p));
}


dynamic_bitset<> cat_get_default_schemata()
{
	uint64_t mask;
	std::ifstream f = open_ifstream(ROOT"/info/L3/cbm_mask");
    f >> std::hex >> mask;
	return dynamic_bitset<>(__builtin_popcount(mask), mask);

}


void cat_reset()
{
	fs::current_path(ROOT);

	cos_delete_all();

	// Reset schemata
	cos_set_schemata(".", cat_get_default_schemata());

	// Reset cpus
	cos_set_cpus(".", dynamic_bitset<>(MAX_CPUS, -1ul));
}
