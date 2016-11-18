#include <iostream>

#include <yaml-cpp/yaml.h>

#include "cat-policy.hpp"
#include "config.hpp"


using std::vector;
using std::string;


static std::shared_ptr<CAT_Policy> config_read_cat_policy(const YAML::Node &config);
static vector<Cos> config_read_cos(const YAML::Node &config);
static vector<Task> config_read_tasks(const YAML::Node &config);


static
std::shared_ptr<CAT_Policy> config_read_cat_policy(const YAML::Node &config)
{
	YAML::Node policy = config["cat_policy"];

	if (!policy["kind"])
		throw std::runtime_error("The CAT policy needs a 'kind' field");
	string kind = policy["kind"].as<string>();

	if (kind == "slowfirst")
	{
		// Check that required fields exist
		for (string field : {"every", "cos"})
			if (!policy[field])
				throw std::runtime_error("The '" + kind + "' CAT policy needs the '" + field + "' field");

		// Read fields
		uint64_t every = policy["every"].as<uint64_t>();
		auto masks = vector<uint64_t>();
		for (const auto &node : policy["cos"])
			masks.push_back(node.as<uint64_t>());
		if (masks.size() <= 2)
			throw std::runtime_error("The '" + kind + "' CAT policy needs at least two COS");

		return std::make_shared<CAT_Policy_Slowfirst>(every, masks);
	}

	else
		throw std::runtime_error("Unknown CAT policy: '" + kind + "'");
}


static
vector<Cos> config_read_cos(const YAML::Node &config)
{
	YAML::Node cos_section = config["cos"];
	auto result = vector<Cos>();

	if (cos_section.Type() != YAML::NodeType::Sequence)
		throw std::runtime_error("In the config file, the cos section must contain a sequence");

	for (size_t i = 0; i < cos_section.size(); i++)
	{
		const auto &cos = cos_section[i];

		// Schematas are mandatory
		if (!cos["schemata"])
			throw std::runtime_error("Each cos must have an schemata");
		auto mask = cos["schemata"].as<uint64_t>();

		// CPUs are not mandatory, but note that COS 0 will have all the CPUs by defect
		auto cpus = vector<uint32_t>();
		if (cos["cpus"])
		{
			auto cpulist = cos["cpus"];
			for (auto it = cpulist.begin(); it != cpulist.end(); it++)
			{
				int cpu = it->as<int>();
				cpus.push_back(cpu);
			}
		}

		result.push_back(Cos(mask, cpus));
	}

	return result;
}


static
vector<Task> config_read_tasks(const YAML::Node &config)
{
	YAML::Node tasks = config["tasks"];
	auto result = vector<Task>();
	for (size_t i = 0; i < tasks.size(); i++)
	{
		if (!tasks[i]["app"])
			throw std::runtime_error("Each task must have an app dictionary with at leask the key 'cmd', and optionally the keys 'stdout', 'stdin', 'stderr', 'skel' and 'max_instr'");

		const auto &app = tasks[i]["app"];

		// Commandline
		if (!app["cmd"])
			throw std::runtime_error("Each task must have a cmd");
		string cmd = app["cmd"].as<string>();

		// Dir containing files to copy to rundir
		string skel = app["skel"] ? app["skel"].as<string>() : "";

		// Stdin/out/err redirection
		string output = app["stdout"] ? app["stdout"].as<string>() : "out";
		string input = app["stdin"] ? app["stdin"].as<string>() : "";
		string error = app["stderr"] ? app["stderr"].as<string>() : "err";

		// CPU affinity
		if (!tasks[i]["cpu"])
			throw std::runtime_error("Each task must have a cpu");
		auto cpu = tasks[i]["cpu"].as<uint32_t>();

		// Maximum number of instructions to execute
		uint64_t max_instr = tasks[i]["max_instr"] ? tasks[i]["max_instr"].as<uint64_t>() : -1ULL;

		bool batch = tasks[i]["batch"] ? tasks[i]["batch"].as<bool>() : false;

		result.push_back(Task(cmd, cpu, output, input, error, skel, max_instr, batch));
	}
	return result;
}


void config_read(const string &path, vector<Task> &tasklist, vector<Cos> &coslist, std::shared_ptr<CAT_Policy> &catpol)
{
	// The message outputed by YAML is not clear enough, so we test first
	std::ifstream f(path);
	if (!f.good())
		throw std::runtime_error("File doesn't exist or is not readable");

	YAML::Node config = YAML::LoadFile(path);

	// Read initial CAT config
	if (config["cos"])
		coslist = config_read_cos(config);

	// Read CAT policy
	if (config["cat_policy"])
		catpol = config_read_cat_policy(config);

	// Read tasks into objects
	if (config["tasks"])
		tasklist = config_read_tasks(config);

	// Check that all COS (but 0) have cpus or tasks assigned
	for (size_t i = 1; i < coslist.size(); i++)
	{
		const auto &cos = coslist[i];
		if (cos.cpus.empty())
			std::cerr << "Warning: COS " + std::to_string(i) + " has no assigned CPUs" << std::endl;
	}
}
