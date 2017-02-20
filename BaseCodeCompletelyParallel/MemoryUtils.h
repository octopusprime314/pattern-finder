#pragma once

#if defined(_WIN64) || defined(_WIN32)
	/* Microsoft Windows (32-bit). or 64 bit ------------------------------ */
	#include "windows.h"
    #include "psapi.h"
#elif defined(__linux__)
	/* Linux. --------------------------------------------------- */
	#include "sys/types.h"
    #include "sys/sysinfo.h"
	#include <sys/wait.h>
	#include <unistd.h>
	#include "sys/times.h"
	#include "sys/vtimes.h"
#endif
#include <string.h>
#include <sstream>
#include "Logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "TypeDefines.h"
using namespace std;

static bool init = false;
#if defined(_WIN64) || defined(_WIN32)
	static ULARGE_INTEGER lastCPU, lastSysCPU, lastUserCPU;
	static int numProcessors;
	static HANDLE self;
	static SYSTEM_INFO sysInfo;
	static FILETIME ftime, fsys, fuser;
#elif defined(__linux__)
	static clock_t lastCPU, lastSysCPU, lastUserCPU;
	static int numProcessors;
#endif
class MemoryUtils
{
public:
	#pragma region MemoryUtilities

	static void print_trace()
	{
		
#if defined(_WIN64) || defined(_WIN32)
		//Dunno
#elif defined(__linux__)
		char pid_buf[30];
		sprintf(pid_buf, "%d", getpid());
		char name_buf[512];
		name_buf[readlink("/proc/self/exe", name_buf, 511)]=0;
		int child_pid = fork();
		if (!child_pid) 
		{           
			dup2(2,1); // redirect output to stderr
			fprintf(stdout,"stack trace for %s pid=%s\n",name_buf,pid_buf);
			execlp("gdb", "gdb", "--batch", "-n", "-ex", "thread", "-ex", "bt", name_buf, pid_buf, NULL);
			abort(); /* If gdb failed to start */
		} else
		{
			waitpid(child_pid,NULL,0);
		}
#endif
	}

	static double GetAvailableRAMMB()
	{

#if defined(_WIN64) || defined(_WIN32)

		MEMORYSTATUSEX statex;
		statex.dwLength = sizeof (statex);
		GlobalMemoryStatusEx (&statex);
		return (double)(statex.ullAvailPhys/(1024.0f*1024.0f));
		
#elif defined(__linux__)

		struct sysinfo info;
		sysinfo(&info);
		return info.freeram/(1024.0f*1024.0f);

#endif

	}

	static PListType parseLine(char* line)
	{
		PListType i = strlen(line);
		while (*line < '0' || *line > '9') line++;
		line[i-3] = '\0';
		i = atoi(line);
		return i;
	}
	
	static double GetProgramMemoryConsumption(PListType level = 0)
	{

#if defined(_WIN64) || defined(_WIN32)
		
		PROCESS_MEMORY_COUNTERS pmc;
		GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
		PListType physMemUsedByMe = pmc.WorkingSetSize;
		return physMemUsedByMe/1000000.0f;

#elif defined(__linux__)
		
		FILE* file = fopen("/proc/self/status", "r");
		int result = -1;
		char line[128];
    
		while (fgets(line, 128, file) != NULL){
			if (strncmp(line, "VmRSS:", 6) == 0){
				result = parseLine(line);
				break;
			}
		}
		fclose(file);
		return result/1000.0f;

#endif

	}

	static bool IsOverMemoryCount(double initialMemoryInMB, double memoryBandwidthInMB, double& memoryOverflow)
	{
		double currMemory = GetProgramMemoryConsumption();
		stringstream stringbuilder;
		
		double usedMemory = currMemory - initialMemoryInMB;
		if(usedMemory >= memoryBandwidthInMB)
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	static double CPULoad()
	{
#if defined(_WIN64) || defined(_WIN32)
		
		if(!init)
		{
			GetSystemInfo(&sysInfo);
			numProcessors = sysInfo.dwNumberOfProcessors;

			GetSystemTimeAsFileTime(&ftime);
			memcpy(&lastCPU, &ftime, sizeof(FILETIME));

			self = GetCurrentProcess();
			GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
			memcpy(&lastSysCPU, &fsys, sizeof(FILETIME));
			memcpy(&lastUserCPU, &fuser, sizeof(FILETIME));
			this_thread::sleep_for(std::chrono::milliseconds(500));

			init = true;
		}

		ULARGE_INTEGER now, sys, user;
		double percent;

		GetSystemTimeAsFileTime(&ftime);
		memcpy(&now, &ftime, sizeof(FILETIME));

		GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
		memcpy(&sys, &fsys, sizeof(FILETIME));
		memcpy(&user, &fuser, sizeof(FILETIME));
		percent = (sys.QuadPart - lastSysCPU.QuadPart) +
			(user.QuadPart - lastUserCPU.QuadPart);
		percent /= (now.QuadPart - lastCPU.QuadPart);
		//percent /= numProcessors;
		lastCPU = now;
		lastUserCPU = user;
		lastSysCPU = sys;

		return percent * 100;
#elif defined(__linux__)

	if(!init)
	{
		FILE* file;
		struct tms timeSample;
		char line[128];

		lastCPU = times(&timeSample);
		lastSysCPU = timeSample.tms_stime;
		lastUserCPU = timeSample.tms_utime;

		file = fopen("/proc/cpuinfo", "r");
		numProcessors = 0;
		while(fgets(line, 128, file) != NULL){
			if (strncmp(line, "processor", 9) == 0) numProcessors++;
		}
		fclose(file);
		
		this_thread::sleep_for(std::chrono::milliseconds(500));

		init = true;
	}

    struct tms timeSample;
    clock_t now;
    double percent;

    now = times(&timeSample);
    if (now <= lastCPU || timeSample.tms_stime < lastSysCPU ||
        timeSample.tms_utime < lastUserCPU){
        //Overflow detection. Just skip this value.
        percent = -1.0;
    }
    else{
        percent = (timeSample.tms_stime - lastSysCPU) +
            (timeSample.tms_utime - lastUserCPU);
        percent /= (now - lastCPU);
        //percent /= numProcessors;
        percent *= 100;
    }
    lastCPU = now;
    lastSysCPU = timeSample.tms_stime;
    lastUserCPU = timeSample.tms_utime;

    return percent;
#endif
	}

	static PListType FileSize(string fileName)
	{
		PListType fileSize;

#if defined(_WIN64) || defined(_WIN32)
		
		struct _stat64 st;
		_stat64((char*)fileName.c_str(), &st );
		fileSize = st.st_size;
		
#elif defined(__linux__)
		
		struct stat64 st;
		if(stat64(fileName.c_str(), &st) != 0)
		{
			fileSize = 0;
		}
		else
		{
			fileSize = st.st_size; 
		}
#endif

		return fileSize;
	}

	#pragma endregion MemoryUtilities
};