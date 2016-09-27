#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <boost/dynamic_bitset.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include "cat.hpp"

#define ROOT "/sys/fs/resctrl"
#define MAX_WAYS 20
#define MAX_COS 4
#define MAX_CPUS 24


namespace fs = boost::filesystem;
namespace po = boost::program_options;

using std::string;
using std::vector;
using boost::dynamic_bitset;
using std::cout;
using std::cerr;
using std::endl;


/* Opens an output stream and checks for errors. */
std::ofstream open_ofstream(string path)
{
	std::ofstream f(path);
	if (!f)
	{
		cerr << "ERROR: Cannot open " << path << ". " << strerror(errno) << endl;
		exit(EXIT_FAILURE);
	}
	return f;
}


/* Opens an intput stream and checks for errors. */
std::ifstream open_ifstream(string path)
{
	std::ifstream f(path);
	if (!f)
	{
		cerr << "ERROR: Cannot open " << path << ". " << strerror(errno) << endl;
		exit(EXIT_FAILURE);
	}
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
		f << task << endl;

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


int main(int argc, char **argv)
{
	po::options_description desc("Allowed options");
	desc.add_options()
		("help,h", "print usage message")
		("create,c", po::value<vector<string>>()->multitoken(), "Create COS <name> <schemata_mask> <cpus_mask> [tasks...]")
		("delete,d", po::value<string>(), "Delete COS")
		("reset,r", "Delete all COS and reset default schemata")
		;

	// Parse the options without storing them in a map.
	po::parsed_options parsed_options = po::command_line_parser(argc, argv)
		.options(desc)
		.run();

	po::variables_map vm;
	po::store(parsed_options, vm);
	po::notify(vm);

	if (vm.count("help")) {
		std::cout << desc << "\n";
		return 0;
	}

	if (vm.count("create"))
	{
		vector<string> data = vm["create"].as<vector<string>>();
		if (data.size() < 3)
		{
			cerr << "ERROR: " << "create needs at least 3 arguments" << endl;
			exit(EXIT_FAILURE);
		}

		try
		{
			string cos = data[0];
			unsigned int sch_mask = std::stoi(data[1], 0, 16);
			unsigned int cpu_mask = std::stoi(data[2], 0, 16);
			dynamic_bitset<> sch(MAX_WAYS, sch_mask);
			dynamic_bitset<> cpu(MAX_WAYS, cpu_mask);
			cos_create(cos, sch, cpu, vector<string>(data.begin() + 3, data.end()));
		}
		catch (const std::exception &e)
		{
			cerr << "ERROR: " << e.what() << endl;
			exit(EXIT_FAILURE);
		}
	}

	if (vm.count("delete"))
	{
		cos_delete(vm["delete"].as<string>());
	}

	if (vm.count("reset"))
	{
		cat_reset();
	}
}
