#include <iostream>

#include <yaml-cpp/yaml.h>
#include <fmt/format.h>

#include "config.hpp"
#include "log.hpp"


using std::vector;
using std::string;
using fmt::literals::operator""_format;


static std::shared_ptr<cat::policy::Base> config_read_cat_policy(const YAML::Node &config);
static vector<Cos> config_read_cos(const YAML::Node &config);
static vector<Task> config_read_tasks(const YAML::Node &config);
static YAML::Node merge(YAML::Node user, YAML::Node def);


static
std::shared_ptr<cat::policy::Base> config_read_cat_policy(const YAML::Node &config)
{
	YAML::Node policy = config["cat_policy"];

	if (!policy["kind"])
		throw_with_trace(std::runtime_error("The CAT policy needs a 'kind' field"));
	string kind = policy["kind"].as<string>();

	if (kind == "none")
		return std::make_shared<cat::policy::Base>();
	
	if (kind == "np" ) // NoPArt Policy
        {
		LOGINF("Using NoPart (np) CAT policy");
		
                // Check that required fields exist
                for (string field : {"every", "stats"})
                        if (!policy[field])
                                throw std::runtime_error("The '" + kind + "' CAT policy needs the '" + field + "' field");

                // Read fields
                uint64_t every = policy["every"].as<uint64_t>();
		string stats = policy["stats"].as<string>();

                return std::make_shared<cat::policy::NoPart>(every, stats);
        }
	
	else if (kind == "hg" ) // HitsGain Policy
        {
                LOGINF("Using HitsGain (hg) CAT policy");

                // Check that required fields exist
                for (string field : {"every", "ways_increment", "stats"})
                        if (!policy[field])
                                throw std::runtime_error("The '" + kind + "' CAT policy needs the '" + field + "' field");
		
                // Read fields
                uint64_t every = policy["every"].as<uint64_t>();
		int ways_increment = policy["ways_increment"].as<int>();
		string stats = policy["stats"].as<string>();

                return std::make_shared<cat::policy::HitsGain>(every, ways_increment, stats);
        }

	else if (kind == "sf" )
	{
		LOGINF("Using Slowfirst (sf) CAT policy");

		// Check that required fields exist
		for (string field : {"every", "cos"})
			if (!policy[field])
				throw_with_trace(std::runtime_error("The '" + kind + "' CAT policy needs the '" + field + "' field"));

		// Read fields
		uint64_t every = policy["every"].as<uint64_t>();
		auto masks = vector<uint64_t>();
		for (const auto &node : policy["cos"])
			masks.push_back(node.as<uint64_t>());
		if (masks.size() <= 2)
			throw_with_trace(std::runtime_error("The '" + kind + "' CAT policy needs at least two COS"));

		return std::make_shared<cat::policy::Slowfirst>(every, masks);
	}

	else if (kind == "sfc" || kind == "sfca")
	{
		// Check that required fields exist
		for (string field : {"every", "cos", "num_clusters"})
			if (!policy[field])
				throw_with_trace(std::runtime_error("The '" + kind + "' CAT policy needs the '" + field + "' field"));

		// Read fields
		uint64_t every = policy["every"].as<uint64_t>();
		auto masks = vector<uint64_t>();
		uint32_t num_clusters = policy["num_clusters"].as<uint32_t>();
		for (const auto &node : policy["cos"])
			masks.push_back(node.as<uint64_t>());
		if (masks.size() <= 2)
			throw_with_trace(std::runtime_error("The '" + kind + "' CAT policy needs at least two COS"));

		if (kind == "sfc")
		{
			LOGINF("Using Slowfirst Clustered (sfc) CAT policy");
			return std::make_shared<cat::policy::SlowfirstClustered>(every, masks, num_clusters);
		}

		LOGINF("Using Slowfirst Clustered and Adjusted (sfca) CAT policy");
		return std::make_shared<cat::policy::SlowfirstClusteredAdjusted>(every, masks, num_clusters);
	}

	else if (kind == "sfcoa")
	{
		vector<string> required = {"kind", "every", "model"};
		vector<string> allowed  = {"num_clusters", "alternate_sides", "min_stall_ratio", "detect_outliers", "eval_clusters", "cluster_sizes", "min_max"};
		vector<std::pair<string, string>> incompatible = {
				{"num_clusters", "eval_clusters"},
				{"num_clusters", "cluster_sizes"},
				{"eval_clusters", "cluster_sizes"},
				{"min_max", "num_clusters"},
				{"min_max", "cluster_sizes"},
				{"min_max", "eval_clusters"},
		};
		allowed.insert(allowed.end(), required.begin(), required.end());

		// Check that required fields exist
		for (string field : required)
			if (!policy[field])
				throw_with_trace(std::runtime_error("The '" + kind + "' CAT policy needs the '" + field + "' field"));

		// Check that all the fields present are allowed
		for (const auto &node : policy)
		{
			string field = node.first.Scalar();
			if (std::find(allowed.begin(), allowed.end(), field) == allowed.end())
				LOGWAR("Field {} is not allowed in the {} policy"_format(field, kind));
		}

		// Check that there are not incompatible fields
		for (const auto &pair : incompatible)
		{
			if (policy[pair.first] && policy[pair.second])
				LOGWAR("Fields {} and {} cannot be used together in the {} policy"_format(pair.first, pair.second, kind));
		}

		// Read fields
		uint64_t every = policy["every"].as<uint64_t>();
		uint32_t num_clusters = policy["num_clusters"] ? policy["num_clusters"].as<uint32_t>() : 0;
		string model = policy["model"].as<string>();
		bool alternate_sides = policy["alternate_sides"] ? policy["alternate_sides"].as<bool>() : false;
		double min_stall_ratio = policy["min_stall_ratio"] ? policy["min_stall_ratio"].as<double>() : 0;
		bool detect_outliers = policy["detect_outliers"] ? policy["detect_outliers"].as<bool>() : false;
		string eval_clusters = policy["eval_clusters"] ? policy["eval_clusters"].as<string>() : "dunn";
		vector<uint32_t> cluster_sizes = policy["cluster_sizes"] ? policy["cluster_sizes"].as<vector<uint32_t>>() : vector<uint32_t>();
		bool min_max = policy["min_max"] ? policy["min_max"].as<bool>() : false;

		LOGINF("Using Slowfirst Clustered Optimally and Adjusted (sfcoa) CAT policy");
		return std::make_shared<cat::policy::SfCOA>(every, num_clusters, model, alternate_sides, min_stall_ratio, detect_outliers, eval_clusters, cluster_sizes, min_max);
	}

	else
		throw_with_trace(std::runtime_error("Unknown CAT policy: '" + kind + "'"));
}


static
vector<Cos> config_read_cos(const YAML::Node &config)
{
	YAML::Node cos_section = config["cos"];
	auto result = vector<Cos>();

	if (cos_section.Type() != YAML::NodeType::Sequence)
		throw_with_trace(std::runtime_error("In the config file, the cos section must contain a sequence"));

	for (size_t i = 0; i < cos_section.size(); i++)
	{
		const auto &cos = cos_section[i];

		// Schematas are mandatory
		if (!cos["schemata"])
			throw_with_trace(std::runtime_error("Each cos must have an schemata"));
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
			throw_with_trace(std::runtime_error("Each task must have an app dictionary with at leask the key 'cmd', and optionally the keys 'stdout', 'stdin', 'stderr', 'skel' and 'max_instr'"));

		const auto &app = tasks[i]["app"];

		// Commandline
		if (!app["cmd"])
			throw_with_trace(std::runtime_error("Each task must have a cmd"));
		string cmd = app["cmd"].as<string>();

		// Dir containing files to copy to rundir
		string skel = app["skel"] ? app["skel"].as<string>() : "";

		// Stdin/out/err redirection
		string output = app["stdout"] ? app["stdout"].as<string>() : "out";
		string input = app["stdin"] ? app["stdin"].as<string>() : "";
		string error = app["stderr"] ? app["stderr"].as<string>() : "err";

		// CPU affinity
		if (!tasks[i]["cpu"])
			throw_with_trace(std::runtime_error("Each task must have a cpu"));
		auto cpu = tasks[i]["cpu"].as<uint32_t>();

		// Maximum number of instructions to execute
		uint64_t max_instr = tasks[i]["max_instr"] ? tasks[i]["max_instr"].as<uint64_t>() : -1ULL;

		bool batch = tasks[i]["batch"] ? tasks[i]["batch"].as<bool>() : false;

		result.push_back(Task(cmd, cpu, output, input, error, skel, max_instr, batch));
	}
	return result;
}


static
YAML::Node merge(YAML::Node user, YAML::Node def)
{
	if (user.Type() == YAML::NodeType::Map && def.Type() == YAML::NodeType::Map)
	{
		for (auto it = def.begin(); it != def.end(); ++it)
		{
			std::string key = it->first.Scalar();
			YAML::Node value = it->second;

			if (!user[key])
				user[key] = value;
			else
				user[key] = merge(user[key], value);
		}
	}
    return user;
}


void config_read(const string &path, const string &overlay, vector<Task> &tasklist, vector<Cos> &coslist, std::shared_ptr<cat::policy::Base> &catpol)
{
	// The message outputed by YAML is not clear enough, so we test first
	std::ifstream f(path);
	if (!f.good())
		throw_with_trace(std::runtime_error("File doesn't exist or is not readable"));

	YAML::Node config = YAML::LoadFile(path);

	if (overlay != "")
	{
		YAML::Node over = YAML::Load(overlay);
		config = merge(over, config);
	}

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
