/*
   Copyright (c) 2009-2015, Intel Corporation
   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 * Neither the name of Intel Corporation nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
// written by Patrick Lu


/*!     \file pcm-core.cpp
  \brief Example of using CPU counters: implements a performance counter monitoring utility for Intel Core, Offcore events
  */
#define HACK_TO_REMOVE_DUPLICATE_ERROR
#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h> // for gettimeofday()
#include <math.h>
#include <iomanip>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <assert.h>
#include <bitset>
#include "cpucounters.h"
#include "utils.h"

#include <vector>
#include <boost/program_options.hpp>
#include <glib.h>

#define PCM_DELAY_DEFAULT 1.0 // in seconds
#define PCM_DELAY_MIN 0.015 // 15 milliseconds is practical on most modern CPUs
#define PCM_CALIBRATION_INTERVAL 50 // calibrate clock only every 50th iteration
#define MAX_CORES 4096
#define MAX_EVENTS 4

using namespace std;
namespace po = boost::program_options;

struct CoreEvent
{
	char name[256];
	uint64 value;
	uint64 msr_value;
	char * description;
} events[MAX_EVENTS];

EventSelectRegister regs[MAX_EVENTS];



	template <class StateType>
void print_custom_stats(const StateType &BeforeState, const StateType &AfterState, bool csv, uint64 txn_rate)
{
	uint64 cycles = getCycles(BeforeState, AfterState);
	uint64 instr = getInstructionsRetired(BeforeState, AfterState);
	if(!csv)
	{
		cout << double(instr)/double(cycles);
		if(txn_rate == 1)
		{
			cout << setw(14) << unit_format(instr);
			cout << setw(11) << unit_format(cycles);
		} else {
			cout << setw(14) << double(instr)/double(txn_rate);
			cout << setw(11) << double(cycles)/double(txn_rate);
		}
	}
	else
	{
		cout << double(instr)/double(cycles) << ",";
		cout << double(instr)/double(txn_rate) << ",";
		cout << double(cycles)/double(txn_rate) << ",";
	}

	for (int i = 0; i < MAX_EVENTS; ++i)
	{
		if(!csv) {
			cout << setw(10);
			if(txn_rate == 1)
				cout << unit_format(getNumberOfCustomEvents(i, BeforeState, AfterState));
			else
				cout << double(getNumberOfCustomEvents(i, BeforeState, AfterState))/double(txn_rate);
		} else {
			cout << double(getNumberOfCustomEvents(i, BeforeState, AfterState)) / double(txn_rate);
			if (i < MAX_EVENTS - 1)
				cout << ",";
		}
	}

	cout << endl;
}

#define EVENT_SIZE 256
void build_event(const char * argv, EventSelectRegister *reg, int idx)
{
	char *token, *subtoken, *saveptr1, *saveptr2;
	char name[EVENT_SIZE], *str1, *str2;
	int j, tmp;
	uint64 tmp2;
	reg->value = 0;
	reg->fields.usr = 1;
	reg->fields.os = 1;
	reg->fields.enable = 1;

	memset(name,0,EVENT_SIZE);
	strncpy(name,argv,EVENT_SIZE-1);

	for (j = 1, str1 = name; ; j++, str1 = NULL) {
		token = strtok_r(str1, "/", &saveptr1);
		if (token == NULL)
			break;
		printf("%d: %s\n", j, token);
		if(strncmp(token,"cpu",3) == 0)
			continue;

		for (str2 = token; ; str2 = NULL) {
			tmp = -1;
			subtoken = strtok_r(str2, ",", &saveptr2);
			if (subtoken == NULL)
				break;
			if(sscanf(subtoken,"event=%i",&tmp) == 1)
				reg->fields.event_select = tmp;
			else if(sscanf(subtoken,"umask=%i",&tmp) == 1)
				reg->fields.umask = tmp;
			else if(strcmp(subtoken,"edge") == 0)
				reg->fields.edge = 1;
			else if(sscanf(subtoken,"any=%i",&tmp) == 1)
				reg->fields.any_thread = tmp;
			else if(sscanf(subtoken,"inv=%i",&tmp) == 1)
				reg->fields.invert = tmp;
			else if(sscanf(subtoken,"cmask=%i",&tmp) == 1)
				reg->fields.cmask = tmp;
			else if(sscanf(subtoken,"in_tx=%i",&tmp) == 1)
				reg->fields.in_tx = tmp;
			else if(sscanf(subtoken,"in_tx_cp=%i",&tmp) == 1)
				reg->fields.in_txcp = tmp;
			else if(sscanf(subtoken,"pc=%i",&tmp) == 1)
				reg->fields.pin_control = tmp;
			else if(sscanf(subtoken,"offcore_rsp=%llx",&tmp2) == 1) {
				if(idx >= 2)
				{
					cerr << "offcore_rsp must specify in first or second event only. idx=" << idx << endl;
					throw idx;
				}
				events[idx].msr_value = tmp2;
			}
			else if(sscanf(subtoken,"name=%255s",events[idx].name) == 1) ;
			else
			{
				cerr << "Event '" << subtoken << "' is not supported. See the list of supported events"<< endl;
				throw subtoken;
			}

		}
	}
	events[idx].value = reg->value;
}


int main(int argc, char * argv[])
{
	set_signal_handlers();

	cerr << endl;
	cerr << " Intel(r) Performance Counter Monitor: Core Monitoring Utility "<< endl;
	cerr << endl;
	cerr << INTEL_PCM_COPYRIGHT << endl;
	cerr << endl;

	double                                  delay = -1.0;
	uint32                                  cur_event = 0;
	bool                                    csv = false;
	long                                    diff_usec = 0; // deviation of clock is useconds between measurements
	uint64                                  txn_rate = 1;
	int                                     calibrated = PCM_CALIBRATION_INTERVAL - 2; // keeps track is the clock calibration needed
	string                                  program = string(argv[0]);
	PCM::ExtendedCustomCoreEventDescription conf;
	bool                                    show_partial_core_output = false;
	std::bitset<MAX_CORES>                  ycores;

	conf.fixedCfg = NULL; // default
	conf.nGPCounters = 4;
	conf.gpCounterCfg = regs;

	PCM *m = PCM::getInstance();

	po::options_description desc("Allowed options");
	desc.add_options()
		("help,h", "print usage message")
		("csv", po::value<string>(), "pathname for output")
		("delay,d", po::value<double>(), "time interval to sample performance counters.")
		("model", "print CPU Model name and exit (used for pmu-query.py)")
		("event,e", po::value<vector<string>>()->composing()->multitoken(), "optional list of custom events to monitor (up to 4)")
		("cores,c", po::value<vector<int>>()->composing()->multitoken(), "enable specific cores to output")
		("run,r", po::value<string>(), "file with lines like: <coreaffinity> <command>")
		("max-intervals", po::value<size_t>()->default_value(numeric_limits<size_t>::max()), "stop after this number of intervals.")
		;

	// Parse the options without storing them in a map.
	po::parsed_options parsed_options = po::command_line_parser(argc, argv)
		.options(desc)
		.run();

	po::variables_map vm;
	po::store(parsed_options, vm);
	po::notify(vm);

	if (vm.count("help")) {
		cout << desc << "\n";
		return 0;
	}

	if (vm.count("csv")) {
		csv = true;
		m->setOutput(vm["csv"].as<string>());
	}

	if (vm.count("delay")) {
		delay = vm["delay"].as<double>();
	}

	if (vm.count("model")) {
		cout << m->getCPUFamilyModelString() << endl;
		exit(EXIT_SUCCESS);
	}

	if (vm.count("cores")) {
		show_partial_core_output = true;
		for (const auto &core_id : vm["cores"].as<vector<int>>()) {
			if(core_id > MAX_CORES) {
				cerr << "Core ID:" << core_id << " exceed maximum range " << MAX_CORES << ", program abort" << endl;
				exit(EXIT_FAILURE);
			}
			ycores.set(core_id, true);
		}

		if(m->getNumCores() > MAX_CORES) {
			cerr << "Error: --cores option is enabled, but #define MAX_CORES " << MAX_CORES << " is less than m->getNumCores() = " << m->getNumCores() << endl;
			cerr << "There is a potential to crash the system. Please increase MAX_CORES to at least " << m->getNumCores() << " and re-enable this option." << endl;
			exit(EXIT_FAILURE);
		}
	}

	if (vm.count("event")) {
		vector<string> events = vm["event"].as<vector<string>>();
		if(events.size() > 4 ) {
			cerr << "At most 4 events are allowed"<< endl;
			exit(EXIT_FAILURE);
		}

		int cur_event = 0;
		for (const auto& event : events)
		{
			try {
				build_event(event.c_str(), &regs[cur_event], cur_event);
				cur_event++;
			} catch (const char * /* str */) {
				exit(EXIT_FAILURE);
			}
		}
	}

	vector<vector<string>> commands;
	vector<int> affinities;
	if (vm.count("run")) {
		string filename = vm["run"].as<string>();
		string line;
		ifstream f;
		f.open(filename);
		while (std::getline(f, line)) {
			istringstream iss(line);
			while (iss.good()) {
				unsigned int core_id = -1;
				string command_line;
				iss >> core_id;
				getline(iss, command_line);
				if(core_id > MAX_CORES) {
					cerr << "Core ID:" << core_id << " exceed maximum range " << MAX_CORES << ", program abort" << endl;
					exit(EXIT_FAILURE);
				}
				int argc;
				char **argv;
				g_shell_parse_argv (command_line.c_str(), &argc, &argv, NULL);
				vector<string> command;
				for (int i=0; i<argc; i++)
					command.push_back(string(argv[i]));
				commands.push_back(command);
				affinities.push_back(core_id);
			}
		}
		f.close();
	}

	conf.OffcoreResponseMsrValue[0] = events[0].msr_value;
	conf.OffcoreResponseMsrValue[1] = events[1].msr_value;

	PCM::ErrorCode status = m->program(PCM::EXT_CUSTOM_CORE_EVENTS, &conf);
	switch (status)
	{
		case PCM::Success:
			break;
		case PCM::MSRAccessDenied:
			cerr << "Access to Intel(r) Performance Counter Monitor has denied (no MSR or PCI CFG space access)." << endl;
			exit(EXIT_FAILURE);
		case PCM::PMUBusy:
			cerr << "Access to Intel(r) Performance Counter Monitor has denied (Performance Monitoring Unit is occupied by other application). Try to stop the application that uses PMU." << endl;
			cerr << "Alternatively you can try to reset PMU configuration at your own risk. Try to reset? (y/n)" << endl;
			char yn;
			std::cin >> yn;
			if ('y' == yn)
			{
				m->resetPMU();
				cerr << "PMU configuration has been reset. Try to rerun the program again." << endl;
			}
			exit(EXIT_FAILURE);
		default:
			cerr << "Access to Intel(r) Performance Counter Monitor has denied (Unknown error)." << endl;
			exit(EXIT_FAILURE);
	}

	cerr << "\nDetected "<< m->getCPUBrandString() << " \"Intel(r) microarchitecture codename "<<m->getUArchCodename()<<"\""<<endl;

	uint64 BeforeTime = 0, AfterTime = 0;
	SystemCounterState SysBeforeState, SysAfterState;
	const uint32 ncores = m->getNumCores();
	std::vector<CoreCounterState> BeforeState, AfterState;
	std::vector<SocketCounterState> DummySocketStates;

	if ( (commands.size() == 0) && (delay <= 0.0) ) {
		// in case external command is provided in command line, and
		// delay either not provided (-1) or is zero
		m->setBlocked(true);
	} else {
		m->setBlocked(false);
	}

	if (csv) {
		if( delay<=0.0 ) delay = PCM_DELAY_DEFAULT;
	} else {
		// for non-CSV mode delay < 1.0 does not make a lot of practical sense:
		// hard to read from the screen, or
		// in case delay is not provided in command line => set default
		if( ((delay<1.0) && (delay>0.0)) || (delay<=0.0) ) delay = PCM_DELAY_DEFAULT;
	}

	cerr << "Update every "<<delay<<" seconds"<< endl;

	std::cout.precision(2);
	std::cout << std::fixed;

	BeforeTime = m->getTickCount();
	m->getAllCounterStates(SysBeforeState, DummySocketStates, BeforeState);

	/* Run commands */
	vector<pid_t> pids;
	if (commands.size()) {
		pids = execute_cmds(commands, affinities);
	}

	if(csv)
		cout << "Interval,Core,IPC,Instructions,Cycles,Event0,Event1,Event2,Event3\n";


	for (size_t interval = 0; interval < vm["max-intervals"].as<size_t>(); interval++)
	{
		if(!csv) cout << std::flush;
		int delay_ms = int(delay * 1000);
		int calibrated_delay_ms = delay_ms;

		// compensation of delay on Linux/UNIX
		// to make the samling interval as monotone as possible
		struct timeval start_ts, end_ts;
		if(calibrated == 0) {
			gettimeofday(&end_ts, NULL);
			diff_usec = (end_ts.tv_sec-start_ts.tv_sec)*1000000.0+(end_ts.tv_usec-start_ts.tv_usec);
			calibrated_delay_ms = delay_ms - diff_usec/1000.0;
		}

		MySleepMs(calibrated_delay_ms);

		calibrated = (calibrated + 1) % PCM_CALIBRATION_INTERVAL;
		if(calibrated == 0) {
			gettimeofday(&start_ts, NULL);
		}

		AfterTime = m->getTickCount();
		m->getAllCounterStates(SysAfterState, DummySocketStates, AfterState);

		for(uint32 i=0;i<cur_event;++i)
		{
			cout <<"Event"<<i<<": "<<events[i].name<<" (raw 0x"<<
				std::hex << (uint32)events[i].value;

			if(events[i].msr_value)
				cout << ", offcore_rsp 0x" << (uint64) events[i].msr_value;
			cout << std::dec << ")" << endl;
		}

		if (!csv) {
			cout << endl;
			cout << "Time elapsed: "<<dec<<fixed<<AfterTime-BeforeTime<<" ms" << endl;
			cout << "txn_rate: " << txn_rate << endl;
			cout << "Core | IPC | Instructions  |  Cycles  | Event0  | Event1  | Event2  | Event3 \n";
		}

		for(uint32 i = 0; i<ncores ; ++i)
		{
			if(m->isCoreOnline(i) == false || (show_partial_core_output && ycores.test(i) == false))
				continue;
			if(csv)
				cout << interval << "," << i << ",";
			else
				cout <<" "<< setw(3) << i << "   " << setw(2) ;
			print_custom_stats(BeforeState[i], AfterState[i], csv, txn_rate);
		}

		if(!csv) {
			cout << "-------------------------------------------------------------------------------------------------------------------\n";
			cout << "   *   ";
			print_custom_stats(SysBeforeState, SysAfterState, csv, txn_rate);
			std::cout << std::endl;
		}


		swap(BeforeTime, AfterTime);
		swap(BeforeState, AfterState);
		swap(SysBeforeState, SysAfterState);

		if ( m->isBlocked() ) {
			// in case PCM was blocked after spawning child application: break monitoring loop here
			break;
		}
	}

	/* Kill children */
	for (const auto &pid : pids)
		kill(pid, SIGKILL);

	exit(EXIT_SUCCESS);
}
