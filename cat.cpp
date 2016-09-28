#include <fstream>
#include <boost/dynamic_bitset.hpp>
#include <boost/filesystem.hpp>
#include "cat.hpp"


#define ROOT "/sys/fs/resctrl"
#define MAX_COS 4


namespace fs = boost::filesystem;

using std::string;
using std::vector;
using boost::dynamic_bitset;


/* Opens an output stream and checks for errors. */
std::ofstream open_ofstream(string path)
{
	std::ofstream f(path);
	if (!f)
		throw std::runtime_error("Could not open " + path + ": " + strerror(errno));
	return f;
}


/* Opens an intput stream and checks for errors. */
std::ifstream open_ifstream(string path)
{
	std::ifstream f(path);
	if (!f)
		throw std::runtime_error("Could not open " + path + ": " + strerror(errno));
	return f;
}


/* Reads a bitset with the assigned CPUs to a COS.
 * Use . for the default COS */
dynamic_bitset<> cos_get_cpus(string cos)
{
	assert(cos != ""); // Use "." for the default COS
	fs::current_path(ROOT);
	fs::current_path(cos);

	unsigned int mask;
	std::ifstream f = open_ifstream("cpus");
    f >> std::hex >> mask;
	return dynamic_bitset<>(MAX_CPUS, mask);
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
	const char *prefix = "L3:0=";
	int mask = schemata.to_ulong();
	int mask_res;

	assert(cos != ""); // Use "." for the default COS
	fs::current_path(ROOT);
	fs::current_path(cos);

	std::ofstream f = open_ofstream("schemata");
    f << prefix << std::hex << mask << std::dec << std::endl;

	mask_res = cos_get_schemata(cos).to_ulong();
	if (mask != mask_res)
		throw std::runtime_error("Could not set schemata mask for COS " + cos);
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
	fs::remove(path);
}


void cat_reset()
{
	fs::current_path(ROOT);

	// Remove all COS
	for(auto &p: fs::directory_iterator("."))
		if (fs::is_directory(p) && fs::basename(p) != "info")
			cos_delete(fs::basename(p));

	// Reset schemata
	cos_set_schemata(".", dynamic_bitset<>(MAX_WAYS, -1ul));
}
