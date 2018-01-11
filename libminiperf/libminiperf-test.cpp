extern "C"
{
#include "libminiperf.h"
#include <unistd.h>
}

int main(int argc, char **argv)
{
	struct perf_evlist* evlist = setup_events(argv[1], argv[2]);
	enable_counters(evlist);

	while(true)
	{
		sleep(1);
		read_counters(evlist, NULL, NULL, NULL, NULL, NULL, NULL);
		print_counters(evlist);
	}

	return 0;
}
