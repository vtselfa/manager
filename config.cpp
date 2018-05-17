#include <iostream>
#include <map>

#include <boost/algorithm/string/replace.hpp>
#include <yaml-cpp/yaml.h>
#include <fmt/format.h>

#include "cat-linux-policy.hpp"
#include "config.hpp"
#include "log.hpp"


using std::vector;
using std::string;
using fmt::literals::operator""_format;


static std::shared_ptr<cat::policy::Base> config_read_cat_policy(const YAML::Node &config);
static vector<Cos> config_read_cos(const YAML::Node &config);
static tasklist_t config_read_tasks(const YAML::Node &config);
static YAML::Node merge(YAML::Node user, YAML::Node def);
static void config_check_required_fields(const YAML::Node &node, const std::vector<string> &required);
static void config_check_fields(const YAML::Node &node, const std::vector<string> &required, std::vector<string> allowed);


static
void config_check_required_fields(const YAML::Node &node, const std::vector<string> &required)
{
	assert(node.IsMap());

	// Check that required fields exist
	for (string field : required)
		if (!node[field])
			throw_with_trace(std::runtime_error("The node '{}' requires the field '{}'"_format(node.Scalar(), field)));
}


static
void config_check_fields(const YAML::Node &node, const std::vector<string> &required, std::vector<string> allowed)
{
	// Allowed is passed by value...
	allowed.insert(allowed.end(), required.begin(), required.end());

	assert(node.IsMap());

	config_check_required_fields(node, required);

	// Check that all the fields present are allowed
	for (const auto &n : node)
	{
		string field = n.first.Scalar();
		if (std::find(allowed.begin(), allowed.end(), field) == allowed.end())
			LOGWAR("Field '{}' is not allowed in the '{}' node"_format(field, node.Scalar()));
	}
}


static
std::shared_ptr<cat::policy::Base> config_read_cat_policy(const YAML::Node &config)
{
	YAML::Node policy = config["cat_policy"];

	if (!policy["kind"])
		throw_with_trace(std::runtime_error("The CAT policy needs a 'kind' field"));
	string kind = policy["kind"].as<string>();

	if (kind == "none")
		return std::make_shared<cat::policy::Base>();

	if (kind == "sfcoa")
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

		config_check_fields(policy, required, allowed);

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

	// Cluster and distribute
	else if (kind == "cad")
	{
		vector<string> required = {"kind", "clustering", "distribution"};
		vector<string> allowed  = {"every"};

		config_check_fields(policy, required, allowed);

		// Read fields
		uint64_t every = policy["every"] ? policy["every"].as<uint64_t>() : 1;

		// Clustering
		assert(policy["clustering"] && policy["clustering"].IsMap());
		string clustering = policy["clustering"]["kind"].as<string>();
		cat::policy::ClusteringBase_ptr_t clustering_ptr;

		if (clustering == "kmeans")
		{
			config_check_fields(policy["clustering"], {"kind", "num_clusters", "event"}, {"max_clusters", "eval_clusters", "sort_clusters"});
			int num_clusters = policy["clustering"]["num_clusters"].as<int>();
			int max_clusters = policy["clustering"]["max_clusters"] ?
					policy["clustering"]["max_clusters"].as<int>() :
					cat_read_info()["L3"].num_closids;
			EvalClusters eval_clusters = str_to_evalclusters(policy["eval_clusters"] ?
					policy["eval_clusters"].as<string>() :
					"dunn");
			auto event = policy["clustering"]["event"].as<string>();
			auto sort = policy["clustering"]["sort_clusters"].as<string>();
			bool sort_ascending = false;
			if (sort == "ascending")
				sort_ascending = true;
			else if (sort != "descending")
				throw_with_trace(std::runtime_error("The value of 'sort_clusters' can only be 'ascending' or 'decending'"));

			clustering_ptr = std::make_shared<cat::policy::Cluster_KMeans>(num_clusters, max_clusters, eval_clusters, event, sort_ascending);
		}
		else if (clustering == "sf")
		{
			config_check_fields(policy["clustering"], {"kind", "cluster_sizes"}, {});
			if (!policy["clustering"]["cluster_sizes"].IsSequence())
				throw_with_trace(std::runtime_error("The field 'cluster_sizes' must be a sequance of integers"));

			auto cluster_sizes = policy["clustering"]["cluster_sizes"].as<vector<int>>();
			clustering_ptr = std::make_shared<cat::policy::Cluster_SF>(cluster_sizes);
		}
		else
		{
			throw_with_trace(std::runtime_error("Unknown clustering policy {}"_format(clustering)));
		}

		// Distribution
		assert(policy["distribution"] && policy["distribution"].IsMap());
		string distribution = policy["distribution"]["kind"].as<string>();
		cat::policy::DistributingBase_ptr_t distribution_ptr;

		if (distribution == "n")
		{
			config_check_fields(policy["distribution"], {"kind", "n"}, {});
			int n = policy["distribution"]["n"].as<int>();
			distribution_ptr = std::make_shared<cat::policy::Distribute_N>(n);
		}
		else if (distribution == "relfunc")
		{
			config_check_fields(policy["distribution"], {"kind"}, {"invert_metric", "min_ways", "max_ways"});
			auto invert_metric = policy["distribution"]["invert_metric"] ?
					policy["distribution"]["invert_metric"].as<bool>() :
					false;
			auto min_ways = policy["distribution"]["min_ways"] ?
					policy["distribution"]["min_ways"].as<int>() :
					2;
			auto max_ways = policy["distribution"]["max_ways"] ?
					policy["distribution"]["max_ways"].as<int>() :
					20;
			distribution_ptr = std::make_shared<cat::policy::Distribute_RelFunc>(min_ways, max_ways, invert_metric);
		}
		else if (distribution == "static")
		{
			auto masks = policy["distribution"]["masks"].as<cbms_t>();
			distribution_ptr = std::make_shared<cat::policy::Distribute_Static>(masks);
		}
		else
		{
			throw_with_trace(std::runtime_error("Unknown distribution policy {}"_format(distribution)));
		}

		LOGINF("Using {} CAT policy"_format(kind));
		return std::make_shared<cat::policy::ClusterAndDistribute>(every, clustering_ptr, distribution_ptr);
	}

	// Square wave
	else if (kind == "squarewave")
	{
		vector<string> required = {"kind", "waves"};
		vector<string> allowed  = {};

		config_check_fields(policy, required, allowed);

		// Read waves
		assert(policy["waves"].IsSequence());
		typedef cat::policy::SquareWave::Wave wave_t;
		auto waves = std::vector<wave_t>();
		required = {"interval", "up", "down"};
		for (auto node : policy["waves"])
		{
			config_check_required_fields(node, required);
			wave_t wave = wave_t(
					node["interval"].as<decltype(wave_t::interval)>(),
					node["up"].as<decltype(wave_t::up)>(),
					node["down"].as<decltype(wave_t::down)>());
			waves.push_back(wave);
		}
		return std::make_shared<cat::policy::SquareWave>(waves);
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
tasklist_t config_read_tasks(const YAML::Node &config)
{
	YAML::Node tasks = config["tasks"];
	auto result = tasklist_t();
	vector<string> required;
	vector<string> allowed;
	for (size_t i = 0; i < tasks.size(); i++)
	{
		required = {"app"};
		allowed  = {"max_instr", "max_restarts", "define", "initial_clos", "cpus"};
		config_check_fields(tasks[i], required, allowed);

		if (!tasks[i]["app"])
			throw_with_trace(std::runtime_error("Each task must have an app dictionary with at least the key 'cmd', and optionally the keys 'stdout', 'stdin', 'stderr', 'skel' and 'max_instr'"));

		const auto &app = tasks[i]["app"];

		required = {"cmd"};
		allowed  = {"name", "skel", "stdin", "stdout", "stderr", "batch"};
		config_check_fields(app, required, allowed);

		// Commandline and name
		if (!app["cmd"])
			throw_with_trace(std::runtime_error("Each task must have a cmd"));
		string cmd = app["cmd"].as<string>();

		// Name defaults to the name of the executable if not provided
		string name = app["name"] ? app["name"].as<string>() : extract_executable_name(cmd);

		// Dir containing files to copy to rundir
		vector<string> skel = {""};
		if (app["skel"])
		{
			if (app["skel"].IsSequence())
				skel = app["skel"].as<vector<string>>();
			else
				skel = {app["skel"].as<string>()};
		}

		// Stdin/out/err redirection
		string output = app["stdout"] ? app["stdout"].as<string>() : "out";
		string input = app["stdin"] ? app["stdin"].as<string>() : "";
		string error = app["stderr"] ? app["stderr"].as<string>() : "err";

		// CPU affinity
		auto cpus = vector<uint32_t>();
		if (tasks[i]["cpus"])
		{
			auto node = tasks[i]["cpus"];
			assert(node.IsScalar() || node.IsSequence());
			if (node.IsScalar())
				cpus = {node.as<decltype (cpus)::value_type>()};
			else
				cpus = node.as<decltype(cpus)>();
		}
		else
		{
			cpus = sched::allowed_cpus();
		}

		// Initial CLOS
		uint32_t initial_clos = tasks[i]["initial_clos"] ? tasks[i]["initial_clos"].as<decltype(initial_clos)>() : 0;
		LOGINF("Initial CLOS {}"_format(initial_clos));

		// String to string replacement, a la C preprocesor, in the 'cmd' option
		auto vars = std::map<string, string>();
		if (tasks[i]["define"])
		{
			auto node = tasks[i]["define"];
			try
			{
				vars = node.as<decltype(vars)>();
			}
			catch(const std::exception &e)
			{
				throw_with_trace(std::runtime_error("The option 'define' should contain a string to string mapping"));
			}

			for (auto it = vars.begin(); it != vars.end(); ++it)
			{
				string key = it->first;
				string value = it->second;
				boost::replace_all(cmd, key, value);
			}
		}

		// Maximum number of instructions to execute
		auto max_instr = tasks[i]["max_instr"] ? tasks[i]["max_instr"].as<uint64_t>() : 0;
		uint32_t max_restarts = tasks[i]["max_restarts"] ? tasks[i]["max_restarts"].as<decltype(max_restarts)>() : std::numeric_limits<decltype(max_restarts)>::max() ;

		bool batch = tasks[i]["batch"] ? tasks[i]["batch"].as<bool>() : false;

		result.push_back(std::make_shared<Task>(name, cmd, initial_clos, cpus, output, input, error, skel, max_instr, max_restarts, batch));
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


static
sched::ptr_t config_read_sched(const YAML::Node &config)
{
	if (!config["sched"])
		return std::make_shared<sched::Base>();

	const auto &sched = config["sched"];

	vector<string> required;
	vector<string> allowed;

	required = {"kind"};
	allowed  = {"allowed_cpus", "every"};

	// Check minimum required fields
	config_check_required_fields(sched, required);

	string kind                   = sched["kind"].as<string>();
	vector<uint32_t> allowed_cpus = sched["allowed_cpus"] ?
			sched["allowed_cpus"].as<decltype(allowed_cpus)>() :
			sched::allowed_cpus(); // All the allowed cpus for this process according to Linux
	uint32_t every = sched["every"] ? sched["every"].as<decltype(every)>() : 1;

	if (kind == "linux")
	{
		if (every != 1)
			LOGDEB("The Linux scheduler ingrores the 'every' option");
		config_check_fields(sched, required, allowed);
		return std::make_shared<sched::Base>(every, allowed_cpus);
	}

	if (kind == "random")
	{
		config_check_fields(sched, required, allowed);
		return std::make_shared<sched::Random>(every, allowed_cpus);
	}

	if (kind == "fair")
	{
		required.push_back("event");
		required.push_back("weights");
		allowed.push_back("at_least_one");
		config_check_fields(sched, required, allowed);

		string event = sched["event"].as<string>();
		std::vector<uint32_t> weights = sched["weights"].as<decltype(weights)>();
		bool at_least_one = sched["at_least_one"] ? sched["at_least_one"].as<bool>() : false;
		return std::make_shared<sched::Fair>(every, allowed_cpus, event, weights, at_least_one);
	}

	throw_with_trace(std::runtime_error("Invalid sched kind '{}'"_format(kind)));
}


static
void config_read_cmd_options(const YAML::Node &config, CmdOptions &cmd_options)
{
	if (!config["cmd"])
		return;

	const auto &cmd = config["cmd"];

	vector<string> required;
	vector<string> allowed;

	required = {};
	allowed  = {"ti", "mi", "event", "cpu-affinity", "cat-impl"};

	// Check minimum required fields
	config_check_fields(cmd, required, allowed);

	if (cmd["ti"])
		cmd_options.ti = cmd["ti"].as<decltype(cmd_options.ti)>();
	if (cmd["mi"])
		cmd_options.mi = cmd["mi"].as<decltype(cmd_options.mi)>();
	if (cmd["event"])
		cmd_options.event = cmd["event"].as<decltype(cmd_options.event)>();
	if (cmd["cpu-affinity"])
		cmd_options.cpu_affinity = cmd["cpu-affinity"].as<decltype(cmd_options.cpu_affinity)>();
	if (cmd["cat-impl"])
		cmd_options.cat_impl = cmd["cat-impl"].as<decltype(cmd_options.cat_impl)>();
}


void config_read(const string &path, const string &overlay, CmdOptions &cmd_options, tasklist_t &tasklist, vector<Cos> &coslist, std::shared_ptr<cat::policy::Base> &catpol, sched::ptr_t &sched)
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

	// Read scheduler
	sched = config_read_sched(config);

	// Read general config
	config_read_cmd_options(config, cmd_options);
}
