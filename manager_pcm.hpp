#include <iostream>
#include <vector>

// Global variables
const int MAX_EVENTS = 4;

void pcm_setup(std::vector<std::string> str_events);
void pcm_before();
void pcm_after(std::ostream &out);
void pcm_clean();


struct Stats
{
	uint64_t ms;
	uint64_t cycles;
	uint64_t instructions;
	uint64_t event[MAX_EVENTS];

	Stats() = default;

	Stats& operator+=(const Stats &o)
	{
		ms           += o.ms;
		cycles       += o.cycles;
		instructions += o.instructions;
		for (int i = 0; i < MAX_EVENTS; i++)
			event[i] += o.event[i];
		return *this;
	}

	friend Stats operator+(Stats a, const Stats &b)
	{
		a += b;
    	return a;
	}

	void print(std::ostream &out, bool csv_format = true) const;
};

std::vector<Stats> pcm_after(const std::vector<uint32_t> &cores);
