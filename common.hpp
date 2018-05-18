#pragma once

#include <chrono>
#include <fstream>
#include <vector>

#include <boost/filesystem.hpp>


std::ifstream open_ifstream(const boost::filesystem::path &path);
std::ofstream open_ofstream(const boost::filesystem::path &path);

std::string extract_executable_name(const std::string &cmd);
void dir_copy(const std::string &source, const std::string &dest);
void dir_copy_contents(const std::string &source, const std::string &dest);
std::string random_string(size_t length);
void drop_privileges();
void set_cpu_affinity(std::vector<uint32_t> cpus, pid_t pid=0);
void assert_dir_exists(const boost::filesystem::path &dir);

void pid_get_children_rec(const pid_t pid, std::vector<pid_t> &children);


// Measure the time the passed callable object consumes
template<typename TimeT = std::chrono::milliseconds>
struct measure
{
	template<typename F, typename ...Args>
	static typename TimeT::rep execution(F func, Args&&... args)
	{
		namespace chr = std::chrono;
		auto start = chr::system_clock::now();

		// Now call the function with all the parameters you need.
		func(std::forward<Args>(args)...);

		auto duration = chr::duration_cast<TimeT>
			(chr::system_clock::now() - start);

		return duration.count();
	}
};
