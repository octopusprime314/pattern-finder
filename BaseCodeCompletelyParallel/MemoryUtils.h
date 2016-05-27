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
#endif
#include <string.h>
#include <sstream>
#include "Logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "TypeDefines.h"
using namespace std;
typedef std::map<PatternType, vector<PListType>*>::iterator it_map_list_p_type;

class MemoryUtils
{
public:
	#pragma region MemoryUtilities

	static bool fileExists(const std::string& filename)
	{
		struct stat buf;
		if (stat(filename.c_str(), &buf) != -1)
		{
			return true;
		}
		return false;
	}

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
		if (!child_pid) {           
			dup2(2,1); // redirect output to stderr
			fprintf(stdout,"stack trace for %s pid=%s\n",name_buf,pid_buf);
			execlp("gdb", "gdb", "--batch", "-n", "-ex", "thread", "-ex", "bt", name_buf, pid_buf, NULL);
			abort(); /* If gdb failed to start */
		} else {
			waitpid(child_pid,NULL,0);
		}
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

	static double SizeOfVector(vector<vector<PListType>*>* vectorArray)
	{
		
		PListType totalMemoryInBytes = 0;
		for(int i = 0; i < vectorArray->size(); i++)
		{
			totalMemoryInBytes = (*vectorArray)[i]->capacity() * sizeof(PListType) + sizeof(vector<PListType>*);
		}
		double sizeInMB = totalMemoryInBytes/1000000.0f;
		return sizeInMB;
	}

	static double SizeOfMap(map<PatternType, vector<PListType>*> mapFile)
	{
		
		//16 bytes for map itself
		PListType totalMemoryInBytes = 16;
		//Size needed for each node in the map overhead essentially
		totalMemoryInBytes += (mapFile.size() + 1)*32;

		unsigned int mapHashSize = 0;
		if(mapFile.size() > 0)
		{
			mapHashSize = mapFile.begin()->first.length();
			totalMemoryInBytes += (mapFile.size() + 1)*mapHashSize;

			for(it_map_list_p_type iterator = mapFile.begin(); iterator != mapFile.end(); iterator++)
			{
				//Add 24 for the overhead storage for a vector in addition to its capacity
				totalMemoryInBytes += iterator->second->capacity()*sizeof(PListType) + 24;
			}
		}
		
		
		double sizeInMB = totalMemoryInBytes/1000000.0f;
		return sizeInMB;
	}

	static bool IsOverMemoryCount(unsigned long initialMemoryInMB, unsigned long memoryBandwidthInMB, double& memoryOverflow)
	{
		double currMemory = GetProgramMemoryConsumption();
		stringstream stringbuilder;
		
		double usedMemory = currMemory - initialMemoryInMB;
		//cout << usedMemory << endl;
		if(usedMemory >= memoryBandwidthInMB)
		{
			
			memoryOverflow = usedMemory - memoryBandwidthInMB;
			stringbuilder << "Memory overused is " << memoryOverflow << " MB" << endl;
			Logger::WriteLog(stringbuilder.str());
			
			return true;
		}
		else
		{
			return false;
		}
	}

	static bool IsLessThanMemoryCount(unsigned long initialMemoryInMB, unsigned long memoryBandwidthInMB)
	{
		double currMemory = GetProgramMemoryConsumption();

		if(initialMemoryInMB - currMemory >= memoryBandwidthInMB)
		{
			//cout << "Under memory bandwidth" << endl;
			return true;
		}
		else
		{
			//cout << "Over memory bandwidth" << endl;
			return false;
		}
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

	static const size_t BUF_SIZE = 8192;

	static void copy(std::istream& is, std::ostream& os)
	{
		size_t len;
		char buf[BUF_SIZE];

		while((len = is.readsome(buf, BUF_SIZE)) > 0)
		{
			os.write(buf, len);
			os.flush();
		}
	}

	static bool copyFileOver(string fileToCopyFrom, string fileToCopyTo)
	{
		string copyFrom = "../Log/";
		copyFrom.append(fileToCopyFrom);
		copyFrom.append(".txt");
	
		string copyTo = "../Log/";
		copyTo.append(fileToCopyTo);
		copyTo.append(".txt");

#if defined(_WIN64) || defined(_WIN32)
		std::ifstream  src(copyFrom, std::ios::binary);
		std::ofstream  dst(copyTo,   std::ios::binary);
		/*copy(src, dst);
		dst.close();
		src.close();*/
		dst << src.rdbuf();
		dst.flush();
		dst.close();
		src.close();
		//CopyFile(copyFrom.c_str(), copyTo.c_str(), false);
#elif defined(__linux__)
		//Implement later
		std::ifstream  src(copyFrom, std::ios::binary);
		std::ofstream  dst(copyTo,   std::ios::binary);
		dst << src.rdbuf();
#endif
	
		return true;
	}

	#pragma endregion MemoryUtilities
};