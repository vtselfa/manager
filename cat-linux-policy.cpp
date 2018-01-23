#include <algorithm>
#include <cmath>
#include <iostream>
#include <iterator>
#include <memory>
#include <tuple>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/max.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/variance.hpp>
#include <fmt/format.h>

#include "cat-linux-policy.hpp"
#include "kmeans.hpp"
#include "log.hpp"


namespace cat
{
namespace policy
{


namespace acc = boost::accumulators;
using fmt::literals::operator""_format;

// No Part Policy
void NoPart::apply(uint64_t current_interval, const tasklist_t &tasklist)
{
    // Apply only when the amount of intervals specified has passed
    if (current_interval % every != 0)
        return;

    double ipcTotal = 0;

    LOGINF("CAT Policy name: NoPart");
    LOGINF("Using {} stats"_format(stats));

	for (const auto &task_ptr : tasklist)
	{
		const Task &task = *task_ptr;
		std::string taskName = task.name;

		double inst = 0, cycl = 0, ipc = -1;

        assert((stats == "total") | (stats == "interval"));

        if (stats == "total")
        {
            // Cycles and IPnC
            inst = task.stats.sum("instructions");
			cycl = task.stats.sum("cycles");

        }
        else if (stats == "interval")
        {
            // Cycles and IPnC
            inst = task.stats.last("instructions");
			cycl = task.stats.last("cycles");
        }

		ipc = inst / cycl;
        ipcTotal += ipc;
	}
	LOGINF("expected IPC total of this interval = {}"_format(expected_IPC));
    LOGINF("real ipcTotal of this interval = {}"_format(ipcTotal));

	// We assume 4% error
    double max = expected_IPC * 1.04;
    double min = expected_IPC * 0.96;
	LOGINF("min {}, max {}"_format(min,max));

    if(ipcTotal <= max && ipcTotal >= min)
    {
        LOGINF("Prediction is GOOD");
    }
    else
    {
        LOGINF("Prediction is BAD");
    }

    double rel_error = ((double) expected_IPC - (double) ipcTotal) / (double) ipcTotal;
    LOGINF("Pred rel error {}"_format(rel_error));

	// Assign total IPC of this interval to previos value
	expected_IPC = ipcTotal;
}


// Auxiliar method of Critical Alone policy to reset configuration
void CriticalAlone::reset_configuration(const tasklist_t &tasklist)
{
    //assign all tasks to CLOS 1
	for (const auto &task_ptr : tasklist)
    {
		const Task &task = *task_ptr;
        pid_t taskPID = task.pid;
		get_cat()->add_task(1,taskPID);
	}

    //change masks of CLOS to 0xfffff
    get_cat()->set_cbm(1,0xfffff);
    get_cat()->set_cbm(2,0xfffff);

    firstTime = 1;
    state = 0;
    expectedIPCtotal = 0;

    maskCrCLOS = 0xfffff;
    maskNonCrCLOS = 0xfffff;

    num_ways_CLOS_2 = 20;
    num_ways_CLOS_1 = 20;

    num_shared_ways = 0;

    idle = false;
    idle_count = IDLE_INTERVALS;

    LOGINF("Reset performed. Original configuration restored");
}

double CriticalAlone::medianV(std::vector<pairD_t> &vec)
{
	double med;
    size_t size = vec.size();

    if (size  % 2 == 0)
    {
    	med = (std::get<1>(vec[size/2 - 1]) + std::get<1>(vec[size/2])) / 2;
    }
    else
    {
    	med = std::get<1>(vec[size/2]);
    }
	return med;
}

void CriticalAlone::apply(uint64_t current_interval, const tasklist_t &tasklist)
{
    LOGINF("Current_interval = {}"_format(current_interval));
    // Apply only when the amount of intervals specified has passed
    if (current_interval % every != 0)
        return;

    // Define accumulators
    //acc::accumulator_set <double, acc::stats<acc::tag::rolling_mean>> macc(acc::tag::rolling_window::window_size = 10u);
    //acc::accumulator_set<double, acc::stats<acc::tag::variance>> accum;

    // (Core, MPKI-L3) tuple
	auto v = std::vector<pairD_t>();
    auto v_ipc = std::vector<pairD_t>();

	auto outlier = std::vector<pair_t>();

    double ipcTotal = 0, mpkiL3Total=0;
    //double missesL3Total = 0, instsTotal = 0;
	double ipc_CR = 0;
    double ipc_NCR = 0;

    uint64_t newMaskNonCr, newMaskCr;

    // Number of critical apps found in the interval
    uint32_t critical_apps = 0;
    bool change_in_outliers = false;

    LOGINF("CAT Policy name: Critical Alone");

	// Gather data
	for (const auto &task_ptr : tasklist)
    {
    	const Task &task = *task_ptr;
        std::string taskName = task.name;
		pid_t taskPID = task.pid;

		// stats per interval
		uint64_t l3_miss = task.stats.last("mem_load_uops_retired.l3_miss");
		uint64_t inst = task.stats.last("instructions");
		double ipc = task.stats.last("ipc");

		double MPKIL3 = (double)(l3_miss*1000) / (double)inst;

		//LOGINF("IPC of task {} is {}"_format(taskPID,ipc));
        LOGINF("Task {} has {} MPKI in L3"_format(taskName,MPKIL3));
        v.push_back(std::make_pair(taskPID, MPKIL3));
        v_ipc.push_back(std::make_pair(taskPID, ipc));

        ipcTotal += ipc;
		mpkiL3Total += MPKIL3;
	}

	//LOGINF("Total IPC of interval {} = {:.2f}"_format(current_interval, ipcTotal));

    // calculate total MPKIL3 mean of interval
	double meanMPKIL3Total = mpkiL3Total / tasklist.size();
	LOGINF("Mean MPKIL3Total (/{}) = {}"_format(tasklist.size(), meanMPKIL3Total));

    // PROCESS DATA
    if (current_interval >= firstInterval)
    {
		// MEAN AND STD LIMIT OUTLIER CALCULATION
		//accumulate value
		macc(meanMPKIL3Total);

		//calculate rolling mean
		mpkiL3Mean = acc::rolling_mean(macc);
     	LOGINF("Rolling mean of MPKI-L3 at interval {} = {}"_format(current_interval, mpkiL3Mean));

		//calculate rolling std and limit of outlier
		stdmpkiL3Mean = std::sqrt(acc::rolling_variance(macc));
		LOGINF("stdMPKIL3mean = {}"_format(stdmpkiL3Mean));

		//calculate limit outlier
		double limit_outlier = mpkiL3Mean + 3*stdmpkiL3Mean;
		LOGINF("limit_outlier = {}"_format(limit_outlier));


		pid_t pidTask;
		//Check if MPKI-L3 of each APP is 2 stds o more higher than the mean MPKI-L3
        for (const auto &item : v)
        {
            double MPKIL3Task = std::get<1>(item);
            pidTask = std::get<0>(item);
			int freqCritical = -1;
			double fractionCritical = 0;

			if(current_interval > firstInterval)
			{
				//Search for mi tuple and update the value
				auto it = frequencyCritical.find(pidTask);
				if(it != frequencyCritical.end())
				{
					freqCritical = it->second;
				}
				else
				{
					LOGINF("TASK RESTARTED --> INCLUDE IT AGAIN IN frequencyCritical");
					frequencyCritical[pidTask] = 0;
					freqCritical = 0;
				}
				assert(freqCritical>=0);
				fractionCritical = freqCritical / (double)(current_interval-firstInterval);
				LOGINF("XXX {} / {} = {}"_format(freqCritical,(current_interval-firstInterval),fractionCritical));
			}


            if (MPKIL3Task >= limit_outlier)
            {
                LOGINF("The MPKI-L3 of task with pid {} is an outlier, since {} >= {}"_format(pidTask,MPKIL3Task,limit_outlier));
                outlier.push_back(std::make_pair(pidTask,1));
                critical_apps = critical_apps + 1;

				// increment frequency critical
				frequencyCritical[pidTask]++;
            }
            else if(MPKIL3Task < limit_outlier && fractionCritical>=0.5)
			{
				LOGINF("The MPKI-L3 of task with pid {} is NOT an outlier, since {} < {}"_format(pidTask,MPKIL3Task,limit_outlier));
				LOGINF("Fraction critical of {} is {} --> CRITICAL"_format(pidTask,fractionCritical));

				outlier.push_back(std::make_pair(pidTask,1));
                critical_apps = critical_apps + 1;
			}
			else
            {
				// it's not a critical app
				LOGINF("The MPKI-L3 of task with pid {} is NOT an outlier, since {} < {}"_format(pidTask,MPKIL3Task,limit_outlier));
                outlier.push_back(std::make_pair(pidTask,0));

				// initialize counter if it's the first interval
				if(current_interval == firstInterval)
				{
					frequencyCritical[pidTask] = 0;
				}
            }
        }

		LOGINF("critical_apps = {}"_format(critical_apps));

        //check CLOS are configured to the correct mask
        if (firstTime)
        {
            // set ways of CLOS 1 and 2
            switch ( critical_apps )
            {
                case 1:
                    // 1 critical app = 12cr10others
                    maskCrCLOS = 0xfff00;
                    num_ways_CLOS_2 = 12;
                    maskNonCrCLOS = 0x003ff;
                    num_ways_CLOS_1 = 10;
                    state = 10;
                    break;
                case 2:
                    // 2 critical apps = 13cr9others
                    maskCrCLOS = 0xfff80;
                    num_ways_CLOS_2 = 13;
                    maskNonCrCLOS = 0x001ff;
                    num_ways_CLOS_1 = 9;
                    state = 20;
                    break;
                case 3:
                    // 3 critical apps = 14cr8others
                    maskCrCLOS = 0xfffc0;
                    num_ways_CLOS_2 = 14;
                    maskNonCrCLOS = 0x000ff;
                    num_ways_CLOS_1 = 8;
                    state = 30;
                    break;
                default:
                     // no critical apps or more than 3 = 20cr20others
                     maskCrCLOS = 0xfffff;
                     num_ways_CLOS_2 = 20;
                     maskNonCrCLOS = 0xfffff;
                     num_ways_CLOS_1 = 20;
                     state = 40;
                     break;
            } // close switch

            num_shared_ways = 2;
            get_cat()->set_cbm(1,maskNonCrCLOS);
            get_cat()->set_cbm(2,maskCrCLOS);

            LOGINF("COS 2 (CR) now has mask {:#x}"_format(maskCrCLOS));
            LOGINF("COS 1 (non-CR) now has mask {:#x}"_format(maskNonCrCLOS));


            firstTime = 0;
			//assign each core to its corresponding CLOS
            for (const auto &item : outlier)
            {
            	pidTask = std::get<0>(item);
                uint32_t outlierValue = std::get<1>(item);

				auto it = std::find_if(v_ipc.begin(), v_ipc.end(),[&pidTask](const auto& tuple) {return std::get<0>(tuple) == pidTask;});
				double ipcTask = std::get<1>(*it);

                if(outlierValue)
                {
                    get_cat()->add_task(2,pidTask);
                    taskIsInCRCLOS.push_back(std::make_pair(pidTask,2));
                    LOGINF("Task PID {} assigned to CLOS 2"_format(pidTask));
                    ipc_CR += ipcTask;
                }
                else
                {
                    get_cat()->add_task(1,pidTask);
                    taskIsInCRCLOS.push_back(std::make_pair(pidTask,1));
                    LOGINF("Task PID {} assigned to CLOS 1"_format(pidTask));
                    ipc_NCR += ipcTask;
                }
            }
	}
	else
	{
		//check if there is a new critical app
            for (const auto &item : outlier)
            {

				pidTask = std::get<0>(item);
            	uint32_t outlierValue = std::get<1>(item);

				auto it = std::find_if(v_ipc.begin(), v_ipc.end(),[&pidTask](const auto& tuple) {return std::get<0>(tuple) == pidTask;});
				double ipcTask = std::get<1>(*it);

				auto it2 = std::find_if(taskIsInCRCLOS.begin(), taskIsInCRCLOS.end(),[&pidTask](const auto& tuple) {return std::get<0>(tuple) == pidTask;});
				double CLOSvalue = std::get<1>(*it2);


                if(outlierValue && (CLOSvalue == 1))
                {
                    LOGINF("There is a new critical app (outlier {}, current CLOS {})"_format(outlierValue,CLOSvalue));
                    change_in_outliers = true;
                }
                else if(!outlierValue && (CLOSvalue == 2))
                {
                    LOGINF("There is a critical app that is no longer critical)");
                    change_in_outliers = true;
                }
                else if(outlierValue)
                {
                    ipc_CR += ipcTask;
                }
                else
                {
                    ipc_NCR += ipcTask;
                }
            }

			//reset configuration if there is a change in critical apps
            if(change_in_outliers)
            {
                taskIsInCRCLOS.clear();
                reset_configuration(tasklist);

            }
            else if(idle)
            {
                LOGINF("Idle interval {}"_format(idle_count));
                idle_count = idle_count - 1;
                if(idle_count == 0)
                {
                    idle = false;
                    idle_count = IDLE_INTERVALS;
                }

            }
			else if(!idle)
            {
                // if there is no new critical app, modify mask if not done previously
                if(critical_apps>0 && critical_apps<4)
                {

                    //LOGINF("Current state = {}"_format(state));

                    LOGINF("IPC total = {}"_format(ipcTotal));
                    LOGINF("Expected IPC total = {}"_format(expectedIPCtotal));

                    double UP_limit_IPC = expectedIPCtotal * 1.04;
                    double LOW_limit_IPC = expectedIPCtotal  * 0.96;

                    double NCR_limit_IPC = ipc_NCR_prev*0.96;
                    double CR_limit_IPC = ipc_CR_prev*0.96;

                    if(ipcTotal > UP_limit_IPC)
                    {
                        LOGINF("New IPC is BETTER: IPCtotal {} > {}"_format(ipcTotal,UP_limit_IPC));
                    }
                    else if((ipc_CR < CR_limit_IPC) && (ipc_NCR >= NCR_limit_IPC))
                    {
                        LOGINF("WORSE CR IPC: CR {} < {} && NCR {} >= {}"_format(ipc_CR,CR_limit_IPC,ipc_NCR,NCR_limit_IPC));
                    }
                    else if((ipc_NCR < NCR_limit_IPC) && (ipc_CR >= CR_limit_IPC))
                    {
                        LOGINF("WORSE NCR IPC: NCR {} < {} && CR {} >= {}"_format(ipc_NCR,NCR_limit_IPC,ipc_CR,CR_limit_IPC));
                    }
                    else if( (ipc_CR < CR_limit_IPC) && (ipc_NCR < NCR_limit_IPC))
                    {
                        LOGINF("BOTH IPCs are WORSE: CR {} < {} && NCR {} < {}"_format(ipc_CR,CR_limit_IPC,ipc_NCR,NCR_limit_IPC));
                    }
                    else
                    {
                        LOGINF("BOTH IPCs are EQUAL (NOT WORSE)");
                    }

					//transitions switch-case
                    switch (state)
                    {
                        case 10:
                            if(ipcTotal > UP_limit_IPC)
                            {
                                // IPC better
                                idle = true;
                            }
                            else if((ipcTotal <= UP_limit_IPC) && (ipcTotal >= LOW_limit_IPC))
                            {
                                // no change in IPC
                                idle = true;
                            }
                            else if((ipc_NCR < NCR_limit_IPC) && (ipc_CR >= CR_limit_IPC))
                            {
                                // worse NCR IPC
                                state = 2;
                            }
                            else if((ipc_CR < CR_limit_IPC) && (ipc_NCR >= NCR_limit_IPC))
                            {
                                // worse CR IPC
                                state = 1;
                            }
                            else
                            {
                                // both IPC are worse
                                state = 1;
                            }
                            break;

						case 20:
                              if(ipcTotal > UP_limit_IPC)
                              {
                                  // IPC better
                                  idle = true;
                              }
                              else if((ipcTotal <= UP_limit_IPC) && (ipcTotal >= LOW_limit_IPC))
                              {
                                  // no change in IPC
                                  idle = true;
                              }
                              else if((ipc_NCR < NCR_limit_IPC) && (ipc_CR >= CR_limit_IPC))
                              {
                                  // worse NCR IPC
                                  state = 2;
                              }
                              else if((ipc_CR < CR_limit_IPC) && (ipc_NCR >= NCR_limit_IPC))
                              {
                                  // worse CR IPC
                                  state = 1;
                              }
                              else
                              {
                                  // both IPC are worse
                                  state = 1;
                              }
                              break;

                        case 30:
                              if(ipcTotal > UP_limit_IPC)
                              {
                                  // IPC better
                                  idle = true;
                              }
                              else if((ipcTotal <= UP_limit_IPC) && (ipcTotal >= LOW_limit_IPC))
                              {
                                  // no change in IPC
                                  idle = true;
                              }
                              else if((ipc_NCR < NCR_limit_IPC) && (ipc_CR >= CR_limit_IPC))
                              {
                                  // worse NCR IPC
                                  state = 2;
                              }
                              else if((ipc_CR < CR_limit_IPC) && (ipc_NCR >= NCR_limit_IPC))
                              {
                                  // worse CR IPC
                                  state = 1;
                              }
                              else
                              {
                                  // both IPC are worse
                                  state = 1;
                              }
                              break;

						case 1:
                            if(ipcTotal > UP_limit_IPC)
                            {
                                // improvement in IPC
                                idle = true;
                            }
                            else if( (ipcTotal <= UP_limit_IPC) && (ipcTotal >= LOW_limit_IPC))
                            {
                                // no change in IPC
                                idle = true;
                            }
                            else if((ipc_NCR < NCR_limit_IPC) && (ipc_CR >= CR_limit_IPC))
                            {
                                // worse NCR IPC
                                state = 3;
                            }
                            else if((ipc_CR < CR_limit_IPC) && (ipc_NCR >= NCR_limit_IPC))
                            {
                                // worse CR IPC
                                state = 4;
                            }
                            else
                            {
                                // both IPC are worse
                                state = 4;
                            }

                            break;

                        case 2:
                            if(ipcTotal > UP_limit_IPC)
                            {
                                // IPC better
                                idle = true;
                            }
                            else if((ipcTotal <= UP_limit_IPC) && (ipcTotal >= LOW_limit_IPC))
                            {
                                // IPC equal
                                idle = true;
                            }
                            else if((ipc_NCR < NCR_limit_IPC) && (ipc_CR >= CR_limit_IPC))
                            {
                                // NCR worse
                                state = 3;
                            }
                            else if((ipc_CR < CR_limit_IPC) && (ipc_NCR >= NCR_limit_IPC))
                            {
                                // CR worse
                                state = 4;
                            }
                            else
                            {
                                // NCR and CR worse
                                state = 4;
                            }
                            break;

						case 3:
                            if(ipcTotal > UP_limit_IPC)
                            {
                                // IPC better
                                idle = true;
                            }
                            else if((ipcTotal <= UP_limit_IPC) && (ipcTotal >= LOW_limit_IPC))
                            {
                                // IPC equal
                                idle = true;
                            }
                            else if((ipc_NCR < NCR_limit_IPC) && (ipc_CR >= CR_limit_IPC))
                            {
                                // NCR worse
                                state = 2;
                            }
                            else if((ipc_CR < CR_limit_IPC) && (ipc_NCR >= NCR_limit_IPC))
                            {
                                // CR worse
                                state = 1;
                            }
                            else
                            {
                                // both WORSE
                                state = 1;
                            }
                            break;

                        case 4:
                            if(ipcTotal > UP_limit_IPC)
                            {
                                // IPC better
                                idle = true;
                            }
                            else if((ipcTotal <= UP_limit_IPC) && (ipcTotal >= LOW_limit_IPC))
                            {
                                // IPC equal
                                idle = true;
                            }
                            else if((ipc_NCR < NCR_limit_IPC) && (ipc_CR >= CR_limit_IPC))
                            {
                                // NCR worse
                                state = 2;
                            }
                            else if((ipc_CR < CR_limit_IPC) && (ipc_NCR >= NCR_limit_IPC))
                            {
                                // CR worse
                                state = 1;
                            }
                            else
                            {
                                // both WORSE
                                state = 1;
                            }
                            break;
					}

					//states switch-case
                    //LOGINF("New state after transition = {}"_format(state));
                    switch ( state )
                    {
                        case 10:
                            if(idle)
                            {
                                LOGINF("New IPC is better or equal-> {} idle intervals"_format(IDLE_INTERVALS));
                            }
                            else
                            {
                                LOGINF("No action performed");
                            }
                            break;

                        case 20:
                              if(idle)
                              {
                                  LOGINF("New IPC is better or equal-> {} idle intervals"_format(IDLE_INTERVALS));
                              }
                              else
                              {
                                  LOGINF("No action performed");
                              }
                              break;

                        case 30:
                                if(idle)
                                {
                                    LOGINF("New IPC is better or equal-> {} idle intervals"_format(IDLE_INTERVALS));
                                }
                                else
                                {
                                    LOGINF("No action performed");
                                }
                                break;

                        case 1:
                            if(idle)
                            {
                                LOGINF("New IPC is better or equal -> {} idle intervals"_format(IDLE_INTERVALS));
                            }
							else
                            {
                                LOGINF("NCR-- (Remove one shared way from CLOS with non-critical apps)");
                                newMaskNonCr = (maskNonCrCLOS >> 1) | 0x00001;
                                maskNonCrCLOS = newMaskNonCr;
                                get_cat()->set_cbm(1,maskNonCrCLOS);
                            }
                            break;

                        case 2:
                            if(idle)
                            {
                                LOGINF("New IPC is better or equal -> {} idle intervals"_format(IDLE_INTERVALS));
                            }
                            else
                            {
                                LOGINF("CR-- (Remove one shared way from CLOS with critical apps)");
                                newMaskCr = (maskCrCLOS << 1) & 0xfffff;
                                maskCrCLOS = newMaskCr;
                                get_cat()->set_cbm(2,maskCrCLOS);
                            }
                            break;

                        case 3:
                            if(idle)
                            {
                                LOGINF("New IPC is better or equal -> {} idle intervals"_format(IDLE_INTERVALS));
                            }
                            else
                            {
                                LOGINF("NCR++ (Add one shared way to CLOS with non-critical apps)");
                                newMaskNonCr = (maskNonCrCLOS << 1) | 0x00001;
                                maskNonCrCLOS = newMaskNonCr;
                                get_cat()->set_cbm(1,maskNonCrCLOS);
                            }
                            break;

                        case 4:
                            if(idle)
                            {
                                LOGINF("New IPC is better or equal -> {} idle intervals"_format(IDLE_INTERVALS));
                            }
                            else
                            {
                                LOGINF("CR++ (Add one shared way to CLOS with critical apps)");
                                newMaskCr = (maskCrCLOS >> 1) | 0x80000;
                                maskCrCLOS = newMaskCr;
                                get_cat()->set_cbm(2,maskCrCLOS);
                            }
                            break;

						default:
                            break;

                    } //SWITCH

                    num_ways_CLOS_1 = __builtin_popcount(get_cat()->get_cbm(1));
                    num_ways_CLOS_2 = __builtin_popcount(get_cat()->get_cbm(2));

					LOGINF("COS 2 (CR)     has mask {:#x} ({} ways)"_format(get_cat()->get_cbm(2),num_ways_CLOS_2));
                    LOGINF("COS 1 (non-CR) has mask {:#x} ({} ways)"_format(get_cat()->get_cbm(1),num_ways_CLOS_1));

                    num_shared_ways = (num_ways_CLOS_2 + num_ways_CLOS_1) - 20;
                    LOGINF("Number of shared ways: {}"_format(num_shared_ways));
                    assert(num_shared_ways >= 0);

                } //if(critical>0 && critical<4)

            } //else if(!idle)
        }//else no es firstime
        LOGINF("Current state = {}"_format(state));
    }//if (current_interval >= firstInterval)

    //calculate new gradient

    ipc_CR_prev = ipc_CR;
    ipc_NCR_prev = ipc_NCR;

	// Assign total IPC of this interval to previos value
    expectedIPCtotal = ipcTotal;


}//apply




// Assign each task to a cluster
clusters_t ClusteringBase::apply(const tasklist_t &tasklist)
{
	auto clusters = clusters_t();
	for (const auto &task_ptr : tasklist)
	{
		const Task &task = *task_ptr;
		clusters.push_back(Cluster(task.id, {0}));
		clusters[task.id].addPoint(
				std::make_shared<Point>(task.id, std::vector<double>{0}));
	}
	return clusters;
}


clusters_t Cluster_SF::apply(const tasklist_t &tasklist)
{
	// (Pos, Stalls) tuple
	typedef std::pair<pid_t, uint64_t> pair_t;
	auto v = std::vector<pair_t>();

	for (const auto &task : tasklist)
	{
		uint64_t stalls;
		try
		{
			stalls = acc::sum(task->stats.events.at("cycle_activity.stalls_ldm_pending"));
		}
		catch (const std::exception &e)
		{
			std::string msg = "This policy requires the event 'cycle_activity.stalls_ldm_pending'. The events monitorized are:";
			for (const auto &kv : task->stats.events)
				msg += "\n" + kv.first;
			throw_with_trace(std::runtime_error(msg));
		}
		v.push_back(std::make_pair(task->id, stalls));
	}

	// Sort in descending order by stalls
	std::sort(begin(v), end(v),
			[](const pair_t &t1, const pair_t &t2)
			{
				return std::get<1>(t1) > std::get<1>(t2);
			});

	// Assign tasks to clusters
	auto clusters = clusters_t();
	size_t t = 0;
	for (size_t s = 0; s < sizes.size(); s++)
	{
		auto size = sizes[s];
		clusters.push_back(Cluster(s, {0}));
		assert(size > 0);
		while (size > 0)
		{
			auto task_id = v[t].first;
			auto task_stalls = v[t].second;
			clusters[s].addPoint(std::make_shared<Point>(task_id, std::vector<double>{(double) task_stalls}));
			size--;
			t++;
		}
		clusters[s].updateMeans();
	}
	if (t != tasklist.size())
	{
		int diff = (int) t - (int) tasklist.size();
		throw_with_trace(std::runtime_error("This clustering policy expects {} {} tasks"_format(std::abs(diff), diff > 0 ? "more" : "less")));
	}
	return clusters;
}


clusters_t Cluster_KMeans::apply(const tasklist_t &tasklist)
{
	auto data = std::vector<point_ptr_t>();

	// Put data in the format KMeans expects
	for (const auto &task_ptr : tasklist)
	{
		const Task &task = *task_ptr;
		double metric;
		try
		{
			metric = acc::rolling_mean(task.stats.events.at(event));
			metric = std::floor(metric * 100 + 0.5) / 100; // Round positive numbers to 2 decimals
		}
		catch (const std::exception &e)
		{
			std::string msg = "This policy requires the event '{}'. The events monitorized are:"_format(event);
			for (const auto &kv : task.stats.events)
				msg += "\n" + kv.first;
			throw_with_trace(std::runtime_error(msg));
		}

		if (metric == 0)
			throw_with_trace(ClusteringBase::CouldNotCluster("The event '{}' value is 0 for task {}:{}"_format(event, task.id, task.name)));

		data.push_back(std::make_shared<Point>(task.id, std::vector<double>{metric}));
	}

	std::vector<Cluster> clusters;
	if (num_clusters > 0)
	{
		LOGDEB("Enforce {} clusters..."_format(num_clusters));
		KMeans::clusterize(num_clusters, data, clusters, 100);
	}
	else
	{
		assert(num_clusters == 0);
		LOGDEB("Try to find the optimal number of clusters...");
		KMeans::clusterize_optimally(max_clusters, data, clusters, 100, eval_clusters);
	}

	LOGDEB(fmt::format("Clusterize: {} points in {} clusters using the event '{}'", data.size(), clusters.size(), event));

	// Sort clusters in ASCENDING order
	if (sort_ascending)
	{
		std::sort(begin(clusters), end(clusters),
				[this](const Cluster &c1, const Cluster &c2)
				{
					return c1.getCentroid()[0] < c2.getCentroid()[0];
				});
	}

	// Sort clusters in DESCENDING order
	else
	{
		std::sort(begin(clusters), end(clusters),
				[this](const Cluster &c1, const Cluster &c2)
				{
					return c1.getCentroid()[0] > c2.getCentroid()[0];
				});
	}
	LOGDEB("Sorted clusters in {} order:"_format(sort_ascending ? "ascending" : "descending"));
	for (const auto &cluster : clusters)
		LOGDEB(cluster.to_string());

	return clusters;
}


ways_t Distribute_N::apply(const tasklist_t &, const clusters_t &clusters)
{
	ways_t ways(clusters.size(), -1);
	for (size_t i = 0; i < clusters.size(); i++)
	{
		ways[i] <<= ((i + 1) * n);
		ways[i] = cut_mask(ways[i]);
		if (ways[i] == 0)
			throw_with_trace(std::runtime_error("Too many CLOSes ({}) or N too big ({}) have resulted in an empty mask"_format(clusters.size(), n)));
	}
	return ways;
}


ways_t Distribute_RelFunc::apply(const tasklist_t &, const clusters_t &clusters)
{
	ways_t cbms;
	auto values = std::vector<double>();
	if (invert_metric)
		LOGDEB("Inverting metric...");
	for (size_t i = 0; i < clusters.size(); i++)
	{
		if (invert_metric)
			values.push_back(1 / clusters[i].getCentroid()[0]);
		else
			values.push_back(clusters[i].getCentroid()[0]);
	}
	double max = *std::max_element(values.begin(), values.end());

	for (size_t i = 0; i < values.size(); i++)
	{
		double x = values[i] / max;
		double y;
		assert(x >= 0 && x <= 1);
		const double a = std::log(max_ways - min_ways + 1);
		x *= a; // Scale X for the interval [0, a]
		y = std::exp(x) + min_ways - 1;
		int ways = std::round(y);
		cbms.push_back(cut_mask(~(-1 << ways)));

		LOGDEB("Cluster {} : x = {} y = {} -> {} ways"_format(i, x, y, ways));
	}
	return cbms;
}


void ClusterAndDistribute::show(const tasklist_t &tasklist, const clusters_t &clusters, const ways_t &ways)
{
	assert(clusters.size() == ways.size());
	for (size_t i = 0; i < ways.size(); i++)
	{
		std::string task_ids;
		const auto &points = clusters[i].getPoints();
		size_t p = 0;
		for (const auto &point : points)
		{
			const auto &task = tasks_find(tasklist, point->id);
			task_ids += "{}:{}"_format(task->id, task->name);
			task_ids += (p == points.size() - 1) ? "" : ", ";
			p++;
		}
		LOGDEB(fmt::format("{{COS{}: {{mask: {:#7x}, num_ways: {:2}, tasks: [{}]}}}}", i, ways[i], __builtin_popcount(ways[i]), task_ids));
	}
}


void ClusterAndDistribute::apply(uint64_t current_interval, const tasklist_t &tasklist)
{
	// Apply the policy only when the amount of intervals specified has passed
	if (current_interval % every != 0)
		return;

	clusters_t clusters;
	try
	{
		clusters = clustering->apply(tasklist);
	}
	catch (const ClusteringBase::CouldNotCluster &e)
	{
		LOGWAR("Not doing any partitioning in interval {}: {}"_format(current_interval, e.what()));
		return;
	}
	auto ways = distributing->apply(tasklist, clusters);
	show(tasklist, clusters, ways);
	tasks_to_closes(tasklist, clusters);
	ways_to_closes(ways);
}

}} // cat::policy
