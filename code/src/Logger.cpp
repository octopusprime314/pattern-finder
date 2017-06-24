
#include "Logger.h"
#if defined(_WIN64) || defined(_WIN32)
	#include <Windows.h>
#elif defined(__linux__)
	#include <sys/types.h>
	#include <unistd.h>
#endif

//Global files used primarily for matlab processing and logging data
#if defined(_WIN64) || defined(_WIN32)
ofstream* Logger::outputFile = new ofstream(LOGGERPATH + "Log" + GetPID() + ".txt", ios_base::in | ios_base::out | ios_base::trunc);
ofstream* Logger::patternDataFile = new ofstream(CSVPATH + "CollectivePatternData" + GetPID() + ".csv", ios_base::in | ios_base::out | ios_base::trunc);
ofstream* Logger::coverageFile = new ofstream(CSVPATH + "PatternVsFileCoverage" + GetPID() + ".csv" , ios_base::in | ios_base::out | ios_base::trunc);
ofstream* Logger::patternOutputFile = new ofstream(LOGGERPATH + "Output" + GetPID() + ".txt" , ios_base::in | ios_base::out | ios_base::trunc);
#elif defined(__linux__)
ofstream* Logger::outputFile = new ofstream(LOGGERPATH + "Log" + GetPID() + ".txt", ios_base::in | ios_base::out | ios_base::trunc);
ofstream* Logger::patternDataFile = new ofstream(CSVPATH + "CollectivePatternData" + GetPID() + ".txt", ios_base::in | ios_base::out | ios_base::trunc);
ofstream* Logger::coverageFile = new ofstream(CSVPATH + "PatternVsFileCoverage" + GetPID() + ".csv" , ios_base::in | ios_base::out | ios_base::trunc);
ofstream* Logger::patternOutputFile = new ofstream(LOGGERPATH + "Output" + GetPID() + ".txt" , ios_base::in | ios_base::out | ios_base::trunc);

#endif
	

string Logger::stringBuffer;
mutex* Logger::logMutex = new mutex();
int Logger::verbosity = 0;
void Logger::WriteLog(string miniBuff)
{
	if(verbosity)
	{
		if(!disableLogging )
		{
			logMutex->lock();

			//String gets written live to log while 
			//log.txt gets written to at certain hard disk size blocks
			if(cmdEnabled)
			{
				cout << miniBuff;
			}

			stringBuffer.append(miniBuff);
			(*outputFile) << stringBuffer;
			stringBuffer.clear();

			//refresh
			(*outputFile).flush();
			(*outputFile).clear();
		
			logMutex->unlock();
		}
	}
}
string Logger::GetPID()
{
	//Return the program PID to generate unique log files
#if defined(_WIN64) || defined(_WIN32)
	return std::to_string(GetCurrentProcessId());
#elif defined(__linux__)
	return std::to_string(getpid());
#endif
}

//Clear string buffer
void Logger::ClearLog()
{
	(*outputFile) << stringBuffer;
	//If written clear the string out
	stringBuffer.clear();
}


//Close log and save time
void Logger::CloseLog()
{
	logMutex->lock();
	ClearLog(); 
	(*outputFile).close();
	(*patternDataFile).close();
	(*coverageFile).close();
	logMutex->unlock();

	delete outputFile;
	delete logMutex;
	delete patternDataFile;
	delete coverageFile;
}


string Logger::GetFormattedTime()
{
	time_t t = time(0);   // get time now
    struct tm now;
	now = *localtime( &t);
	stringstream timeBuff;

	string amorpm;

	if(now.tm_hour > 0 && now.tm_hour <= 12)
	{
		now.tm_hour = now.tm_hour;
		amorpm = "am";
	}
	else
	{
		now.tm_hour = now.tm_hour%12;
		amorpm = "pm";
	}

	timeBuff << now.tm_hour << "_";
	if(now.tm_min < 10)
	{
		timeBuff << "0";	
	}
	timeBuff << now.tm_min << "_";
	if(now.tm_sec < 10)
	{
		timeBuff << "0";	
	}
	timeBuff << now.tm_sec;
	srand ((unsigned int)(time(NULL)));
	timeBuff << rand();

	return timeBuff.str();
}

string Logger::GetTime()
{
	time_t t = time(0);   // get time now
    struct tm now;
	now = *localtime( &t);
	stringstream timeBuff;

	timeBuff << now.tm_hour << ":";
	if(now.tm_min < 10)
	{
		timeBuff << "0";	
	}
	timeBuff << now.tm_min << ":";
	if(now.tm_sec < 10)
	{
		timeBuff << "0";	
	}
	timeBuff << now.tm_sec;

	return timeBuff.str();
}

void Logger::generateTimeVsFileSizeCSV(vector<double> processTimes, vector<PListType> fileSizes)
{
	//Generates two csv files for large file data set time processing information
	ofstream csvFile(CSVPATH + "TimeVsFileSize" + GetPID() + ".csv" , ios_base::in | ios_base::out | ios_base::trunc);

	if(processTimes.size() == fileSizes.size())
	{
		for(int i = 0; i < processTimes.size(); i++)
		{
			csvFile << processTimes[i] << "," << fileSizes[i] << endl;
		}
	}
	csvFile.close();

	ofstream csvFile2(CSVPATH + "FileNumberVsProcessingTime" + GetPID() + ".csv" , ios_base::in | ios_base::out | ios_base::trunc);
	if(processTimes.size() == fileSizes.size())
	{
		for(int i = 0; i < processTimes.size(); i++)
		{
			csvFile2 << i << "," << processTimes[i] << endl;
		}
	}
	csvFile2.close();
}


void Logger::generateFinalPatternVsCount(map<PListType, PListType> finalPattern)
{
	//Generate large dataset patterns for all files, tries to find patterns that are common between many files
	ofstream csvFile(CSVPATH + "FinalPatternVsCount" + GetPID() + ".csv" , ios_base::in | ios_base::out | ios_base::trunc);

	for(map<PListType, PListType>::iterator it = finalPattern.begin(); it != finalPattern.end(); it++)
	{
		csvFile << it->first << "," << it->second << endl;
	}
	
	csvFile.close();
}

void Logger::generateThreadsVsThroughput(vector<map<int, double>> threadMap)
{
	//Amdahl's law threading comparison
	ofstream csvFile(CSVPATH + "ThreadsVsThroughput" + GetPID() + ".csv" , ios_base::in | ios_base::out | ios_base::trunc);

	for(vector<map<int, double>>::iterator it = threadMap.begin(); it != threadMap.end(); it++)
	{
		for(map<int, double>::iterator it2 = it->begin(); it2 != it->end(); it2++)
		{
			csvFile << it2->first << "," << it->begin()->second/it2->second << endl;
		}
	}
	
	csvFile.close();

	//Generate pure time information associated with number of threads
	ofstream csvFile2(CSVPATH + "ThreadsVsSpeed" + GetPID() + ".csv" , ios_base::in | ios_base::out | ios_base::trunc);

	for(vector<map<int, double>>::iterator it = threadMap.begin(); it != threadMap.end(); it++)
	{
		for(map<int, double>::iterator it2 = it->begin(); it2 != it->end(); it2++)
		{
			csvFile2 << it2->first << "," << it2->second << endl;
		}
	}
	csvFile2.close();

}
void Logger::fileCoverageCSV(const vector<float>& coverage)
{
	//Record the percentage that a pattern covers in a file
	PListType i = 1;
	for(vector<float>::const_iterator it = coverage.begin(); it != coverage.end(); it++)
	{
		(*coverageFile) << i << ",";
		i++;
	}
	(*coverageFile) << endl;

	for(vector<float>::const_iterator it = coverage.begin(); it != coverage.end(); it++)
	{
		(*coverageFile) << *it << ",";
	}
	(*coverageFile) << endl;
}


void Logger::fillPatternData(const string &file, const vector<PListType> &patternIndexes, const vector<PListType> &patternCounts)
{
	//Records generic pattern data for each level including length, pattern string and occurences
	(*patternDataFile) << "Length, Pattern, Count\n";
	int j = 0;
	for(vector<PListType>::const_iterator it = patternIndexes.begin(); it != patternIndexes.end(); it++)
	{
		(*patternDataFile) << j + 1 << ",\"" << file.substr(*it, j + 1) << "\"," << patternCounts[j] << endl;
		j++;
	}
}

