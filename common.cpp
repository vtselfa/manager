#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <glib.h>

#include <fmt/format.h>
#include <grp.h>

#include "common.hpp"
#include "throw-with-trace.hpp"


namespace fs = boost::filesystem;

using fmt::literals::operator""_format;


// Opens an output stream and checks for errors
std::ofstream open_ofstream(const std::string &path)
{
	std::ofstream f;
	f.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	f.open(path);
	return f;
}


// Opens an intput stream and checks for errors
std::ifstream open_ifstream(const std::string &path)
{
	std::ifstream f;
	// Throw on error
	f.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	f.open(path);
	return f;
}


std::ifstream open_ifstream(const boost::filesystem::path &path)
{
	return open_ifstream(path.string());
}


std::ofstream open_ofstream(const boost::filesystem::path &path)
{
	return open_ofstream(path.string());
}


void assert_dir_exists(const boost::filesystem::path &dir)
{
	if (!fs::exists(dir))
		throw_with_trace(std::runtime_error("Dir {} does not exist"_format(dir.string())));
	if (!fs::is_directory(dir))
		throw_with_trace(std::runtime_error("{} is not a directory"_format(dir.string())));
}


// Returns the executable basename from a commandline
std::string extract_executable_name(const std::string &cmd)
{
	int argc;
	char **argv;

	if (!g_shell_parse_argv(cmd.c_str(), &argc, &argv, NULL))
		throw_with_trace(std::runtime_error("Could not parse commandline '" + cmd + "'"));

	std::string result = boost::filesystem::basename(argv[0]);
	g_strfreev(argv); // Free the memory allocated for argv

	return result;
}


void dir_copy(const std::string &source, const std::string &dest)
{
	namespace fs = boost::filesystem;

	if (!fs::exists(source) || !fs::is_directory(source))
		throw_with_trace(std::runtime_error("Source directory " + source + " does not exist or is not a directory"));
	if (fs::exists(dest))
		throw_with_trace(std::runtime_error("Destination directory " + dest + " already exists"));
	if (!fs::create_directories(dest))
		throw_with_trace(std::runtime_error("Cannot create destination directory " + dest));

	typedef fs::recursive_directory_iterator RDIter;
	for (auto it = RDIter(source), end = RDIter(); it != end; ++it)
	{
		const auto &path = it->path();
		auto relpath = it->path().string();
		boost::replace_first(relpath, source, ""); // Convert the path to a relative path

		fs::copy(path, dest + "/" + relpath);
	}
}


std::string random_string(size_t length)
{
	auto randchar = []() -> char
	{
		const char charset[] =
			"0123456789"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz";
		const size_t max_index = (sizeof(charset) - 1);
		return charset[rand() % max_index];
	};
	std::string str(length,0);
	std::generate_n(str.begin(), length, randchar);
	return str;
}


// Drop sudo privileges
void drop_privileges()
{
	const char *uidstr = getenv("SUDO_UID");
	const char *gidstr = getenv("SUDO_GID");
	const char *userstr = getenv("SUDO_USER");

	if (!uidstr || !gidstr || !userstr)
		return;

	const uid_t uid = std::stol(uidstr);
	const gid_t gid = std::stol(gidstr);

	if (uid == getuid() && gid == getgid())
		return;

	if (setgid(gid) < 0)
		throw_with_trace(std::runtime_error("Cannot change gid: " + std::string(strerror(errno))));

	if (initgroups(userstr, gid) < 0)
		throw_with_trace(std::runtime_error("Cannot change group access list: " + std::string(strerror(errno))));

	if (setuid(uid) < 0)
		throw_with_trace(std::runtime_error("Cannot change uid: " + std::string(strerror(errno))));
}


void set_cpu_affinity(std::vector<uint32_t> cpus, pid_t pid)
{
	// All cpus allowed
	if (cpus.size() == 0)
		return;

	// Set CPU affinity
	cpu_set_t mask;
	CPU_ZERO(&mask);
	for (auto cpu : cpus)
		CPU_SET(cpu, &mask);
	if (sched_setaffinity(pid, sizeof(mask), &mask) < 0)
		throw_with_trace(std::runtime_error("Could not set CPU affinity: " + std::string(strerror(errno))));
}
