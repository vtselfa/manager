#include <iostream>
#include <boost/dynamic_bitset.hpp>
#include <boost/program_options.hpp>
#include "cat.hpp"


namespace po = boost::program_options;

using std::string;
using std::vector;
using boost::dynamic_bitset;
using std::cout;
using std::cerr;
using std::endl;


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
