/** @file MemoryUtils.h
 *  @brief Memory Utility to query OS ram, cpu and hard disk usage
 *
 *  Queries for OS ram, cpu and hard disk usage
 *
 *  @author Peter J. Morley (pmorley)
 */
#pragma once
#if defined(_WIN64) || defined(_WIN32)
	#include "windows.h"
    #include "psapi.h"
#elif defined(__linux__)
	#include "sys/types.h"
    #include "sys/sysinfo.h"
	#include <sys/wait.h>
	#include <unistd.h>
	#include "sys/times.h"
	#include "sys/vtimes.h"
#endif
#include "ProcessorStats.h"
#include "ProcessorConfig.h"

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

	/** @brief Queries OS for available ram
	 *  
	 *  Used to find out if the program is using too much memory
	 *
	 *  @return double size of available ram in MB
	 */
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

	/** @brief Parses memory consumption
	 *  
	 *  Used to parse memory consumption
	 *
	 *  @return size_t size of ram in MB
	 */
	static size_t parseLine(char* line)
	{
		size_t i = strlen(line);
		while (*line < '0' || *line > '9') line++;
		line[i-3] = '\0';
		i = atoi(line);
		return i;
	}
	

	/** @brief Queries OS for the program's current memory consumption
	 *  
	 *  Queries OS for the program's current memory consumption
	 *
	 *  @return double size of program's memory consumption in MB
	 */
	static double GetProgramMemoryConsumption(PListType level = 0)
	{
#if defined(_WIN64) || defined(_WIN32)
		PROCESS_MEMORY_COUNTERS pmc;
		GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
		size_t physMemUsedByMe = pmc.WorkingSetSize;
		return physMemUsedByMe/1000000.0f;
#elif defined(__linux__)
		
		FILE* file = fopen("/proc/self/status", "r");
		int result = -1;
		char line[128];
    
		while (fgets(line, 128, file) != nullptr){
			if (strncmp(line, "VmRSS:", 6) == 0){
				result = parseLine(line);
				break;
			}
		}
		fclose(file);
		return result/1000.0f;
#endif
	}

	/** @brief Queries OS for the program's Resident Set Size (RSS)
	 *  
	 *  Queries OS for the program's Resident Set Size (RSS)
	 *
	 *  @return size_t size of program's memory consumption in MB
	 */
	static size_t getCurrentRSS( )
	{
#if defined(_WIN32)
		PROCESS_MEMORY_COUNTERS info;
		GetProcessMemoryInfo( GetCurrentProcess( ), &info, sizeof(info) );
		return (size_t)info.WorkingSetSize;
#elif defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)
		long rss = 0L;
		FILE* fp = nullptr;
		if ( (fp = fopen( "/proc/self/statm", "r" )) == nullptr )
			return (size_t)0L;      /* Can't open? */
		if ( fscanf( fp, "%*s%ld", &rss ) != 1 )
		{
			fclose( fp );
			return (size_t)0L;      /* Can't read? */
		}
		fclose( fp );
		return (size_t)rss * (size_t)sysconf( _SC_PAGESIZE);
#endif
	}

	/** @brief Computes whether the program is over memory limit
	 *  
	 *  Computes whether the program is over memory limit
	 *
	 *  @param initialMemoryInMB inital memory used which includes stack and program size in MB
	 *  @param memoryBandwidthInMB memory allowance in MB
	 *  @param memoryOverflow refence of the amount of memory in MB that is over the limit
	 *  @return bool true if the program is using too much memory
	 */
	static bool IsOverMemoryCount(double initialMemoryInMB, double memoryBandwidthInMB, double& memoryOverflow)
	{
		size_t usedMemory = getCurrentRSS()/1000000;
		if(usedMemory >= memoryBandwidthInMB)
		{
			memoryOverflow = usedMemory - memoryBandwidthInMB;
			return true;
		}
		else
		{
			memoryOverflow = 0;
			return false;
		}
	}

	/** @brief Computes the CPU load consumption
	 *  
	 *  Finds the overall cpu consumption of the program to ensure all cores 
	 *  are being utilized.  For example if a program is using 8 cores/threads
	 *  for processing then the cpu utilization should return 800%.
	 *
	 *  @return double total cpu utilization percentage
	 */
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
		percent = static_cast<double>((sys.QuadPart - lastSysCPU.QuadPart) + (user.QuadPart - lastUserCPU.QuadPart)) / (now.QuadPart - lastCPU.QuadPart);
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
		while(fgets(line, 128, file) != nullptr){
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

	/** @brief Queries the size of the file in bytes that resides on the hard disk
	 *  
	 *  Queries the size of the file in bytes that resides on the hard disk
	 *
	 *  @return PListType size of the files in bytes
	 */
	static PListType FileSize(string fileName)
	{
		size_t fileSize;

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
		return static_cast<PListType>(fileSize);
	}

    static bool DiskOrSysMemProcessing(LevelPackage levelInfo,
        PListType sizeOfPrevPatternCount,
        PListType sizeOfString,
        const ConfigurationParams& config,
        ProcessorStats& stats)
    {
        //Break early if memory usage is predetermined by command line arguments
        if (config.usingPureRAM)
        {
            return false;
        }
        if (config.usingPureHD)
        {
            return true;
        }

        //POTENTIAL PATTERNS equals the previous list times 256 possible byte values but this value can't exceed the file size minus the current level
        PListType potentialPatterns = sizeOfPrevPatternCount * 256;

        if (potentialPatterns > config.currentFile->fileStringSize - stats.GetEradicatedPatterns())
        {
            //Factor in eradicated patterns because those places no longer need to be checked in the file
            potentialPatterns = config.currentFile->fileStringSize - stats.GetEradicatedPatterns();
        }

        PListType linearListPListLengthsContainerSizesForPrevAndNext = (sizeof(PListType)*(sizeOfString) * 2) + (potentialPatterns * sizeof(PListType) * 2);  //Predication for containers just predict they will be the same size thus * 2

        PListType sizeOfProcessedFile = 0;
        if (levelInfo.currLevel <= 2)
        {
            sizeOfProcessedFile = config.currentFile->fileStringSize;
        }
        else
        {
            sizeOfProcessedFile = config.currentFile->fileStringSize / config.numThreads;
        }

        PListType sizeOfGlobalStringConstruct = sizeOfString;
        PListType totalStorageNeeded = (linearListPListLengthsContainerSizesForPrevAndNext + sizeOfProcessedFile + sizeOfGlobalStringConstruct) / 1000000;

        PListType previousLevelMemoryMB = 0;

        double prevMemoryMB = MemoryUtils::GetProgramMemoryConsumption() - config.memoryUsageAtInception;
        if (prevMemoryMB > 0.0f)
        {
            previousLevelMemoryMB = (PListType)prevMemoryMB / config.numThreads;
        }

        double memoryAllowance = 0;
        if (levelInfo.currLevel <= 2)
        {
            memoryAllowance = config.memoryBandwidthMB;
        }
        else
        {
            memoryAllowance = config.memoryBandwidthMB / config.numThreads;
        }

        if (totalStorageNeeded > memoryAllowance)
        {
            Logger::WriteLog("Using HARD DISK! Total size for level ", levelInfo.currLevel, " processing is ", totalStorageNeeded, " MB");
            return true;
        }
        else
        {
            Logger::WriteLog("Using DRAM! Total size for level ", levelInfo.currLevel, " processing is ", totalStorageNeeded, " MB");
            return false;
        }
    }

	#pragma endregion MemoryUtilities
};

