#include <cstring>
#include "common.hpp"

/* Opens an output stream and checks for errors. */
std::ofstream open_ofstream(std::string path, ios_base::openmode mode = ios_base::out)
{
	std::ofstream f;
	f.open(path, mode);
	if (!f)
		throw std::runtime_error("Could not open " + path + ": " + strerror(errno));
	return f;
}


/* Opens an intput stream and checks for errors. */
std::ifstream open_ifstream(std::string path)
{
	std::ifstream f(path);
	if (!f)
		throw std::runtime_error("Could not open " + path + ": " + strerror(errno));
	return f;
}
