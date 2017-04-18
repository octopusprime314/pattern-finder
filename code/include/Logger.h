#ifndef LOGGER_H_
#define LOGGER_H_

#include <iostream>
#include <sstream>
#include <fstream>
#include <mutex>
#include "TypeDefines.h"
#include <vector>
#include <map>
using namespace std;

#define SCROLLCOUNT 25
#if defined(_WIN64) || defined(_WIN32)
	const string LOGGERPATH = "../../Log/";
#elif defined(__linux__)
	const string LOGGERPATH = "../Log/";
#endif

#if defined(_WIN64) || defined(_WIN32)
	const string CSVPATH = "../../Runs/";
#elif defined(__linux__)
	const string CSVPATH = "../Runs/";
#endif

const bool cmdEnabled = false;
const bool scrollEnabled = false;
const bool disableLogging = false;

class Logger
{
public:
	//Write to string buffer, if buffer is larger 
	static void WriteLog(string miniBuff);
	//Clear string buffer
	static void ClearLog();
	//Forces flush all data in string buffer to disk
	static void FlushLog();
	//Close log
	static void CloseLog();

	static string GetFormattedTime();
	static string GetTime();

	static int verbosity;

	static void generateTimeVsFileSizeCSV(vector<double> processTimes, vector<PListType> fileSizes);
	static void generateFinalPatternVsCount(map<PListType, PListType> finalPattern);
	static void generateThreadsVsThroughput(vector<map<int, double>> threadMap);
	static void fillPatternData(const string &file, const vector<PListType> &patternIndexes, const vector<PListType> &patternCounts);
	static void fileCoverageCSV(const vector<float>& coverage);

private:
	static string stringBuffer;
	static ofstream *outputFile;
	static ofstream* patternDataFile;
	static ofstream* coverageFile;
	static mutex* logMutex;
	static mutex* scrollLogMutex;
	static int index;
	
	static string GetPID();
};

#endif