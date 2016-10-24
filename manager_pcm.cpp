#include "manager_pcm.hpp"


void pcm_build_event(const char *event_str, EventSelectRegister &reg, CoreEvent &event)
{
	const int EVENT_SIZE = 256;

	char *token, *subtoken, *saveptr1, *saveptr2;
	char name[EVENT_SIZE], *str1, *str2;
	int j, tmp;
	unsigned long long tmp2;

	reg.value = 0;
	reg.fields.usr = 1;
	reg.fields.os = 1;
	reg.fields.enable = 1;

	memset(name, 0, EVENT_SIZE);
	strncpy(name, event_str, EVENT_SIZE - 1);

	for (j = 1, str1 = name; ; j++, str1 = NULL)
	{
		token = strtok_r(str1, "/", &saveptr1);
		if (token == NULL)
			break;

		if(strncmp(token,"cpu", 3) == 0)
			continue;

		for (str2 = token; ; str2 = NULL)
		{
			tmp = -1;
			subtoken = strtok_r(str2, ",", &saveptr2);
			if (subtoken == NULL)
				break;
			if (sscanf(subtoken, "event=%i", &tmp) == 1)
				reg.fields.event_select = tmp;
			else if (sscanf(subtoken, "umask=%i", &tmp) == 1)
				reg.fields.umask = tmp;
			else if (strcmp(subtoken, "edge") == 0)
				reg.fields.edge = 1;
			else if (sscanf(subtoken, "any=%i", &tmp) == 1)
				reg.fields.any_thread = tmp;
			else if (sscanf(subtoken, "inv=%i", &tmp) == 1)
				reg.fields.invert = tmp;
			else if (sscanf(subtoken, "cmask=%i", &tmp) == 1)
				reg.fields.cmask = tmp;
			else if (sscanf(subtoken, "in_tx=%i", &tmp) == 1)
				reg.fields.in_tx = tmp;
			else if (sscanf(subtoken, "in_tx_cp=%i", &tmp) == 1)
				reg.fields.in_txcp = tmp;
			else if (sscanf(subtoken, "pc=%i", &tmp) == 1)
				reg.fields.pin_control = tmp;
			else if (sscanf(subtoken, "offcore_rsp=%llx", &tmp2) == 1)
			{
				// if(idx >= 2)
				// 	throw std::runtime_error("offcore_rsp must specify in first or second event only. idx=" + idx);
				event.msr_value = tmp2;
			}
			else if (sscanf(subtoken,"name=%255s",event.name) == 1)
			{}
			else
				throw std::runtime_error(std::string("Event '") + subtoken + "' is not supported. See the list of supported events");
		}
	}
	event.value = reg.value;
}


template <class IntType>
inline std::string unit_format(IntType n)
{
    char buffer[1024];
    if (n <= 9999ULL)
    {
        sprintf(buffer, "%4d  ", int32(n));
        return buffer;
    }
    if (n <= 9999999ULL)
    {
        sprintf(buffer, "%4d K", int32(n / 1000ULL));
        return buffer;
    }
    if (n <= 9999999999ULL)
    {
        sprintf(buffer, "%4d M", int32(n / 1000000ULL));
        return buffer;
    }
    if (n <= 9999999999999ULL)
    {
        sprintf(buffer, "%4d G", int32(n / 1000000000ULL));
        return buffer;
    }
    sprintf(buffer, "%4d T", int32(n / (1000000000ULL * 1000ULL)));
    return buffer;
}


void pcm_reset()
{
	PCM::getInstance()->resetPMU();
}


void pcm_cleanup()
{
	PCM::getInstance()->cleanup();
}


void Stats::print(std::ostream &out, bool csv_format) const
{
	using std::setw;
	const double ipc = double(instructions) / double(cycles);
	if (csv_format)
	{
		out << ipc << ",";
		out << instructions << ",";
		out << cycles << ",";
		out << us << ",";
	}
	else
	{
		out << setw(10) << ipc;
		out << setw(14) << unit_format(instructions);
		out << setw(11) << unit_format(cycles);
		out << setw(11) << us;
	}

	for (int i = 0; i < MAX_EVENTS; ++i)
	{
		if(csv_format)
		{
			out << event[i];
			if (i < MAX_EVENTS - 1)
				out << ",";
		}
		else
		{
			out << setw(10);
			out << unit_format(event[i]);
		}
	}
	out << std::endl;
}
