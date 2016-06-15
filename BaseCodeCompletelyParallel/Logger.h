#ifndef LOGGER_H_
#define LOGGER_H_

#include <iostream>
#include <sstream>
#include <fstream>
#include <mutex>
#include "TypeDefines.h"
using namespace std;

#define SCROLLCOUNT 25
const string LOGGERPATH = "../Log/";
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

	static int verbosity;

private:
	static string stringBuffer;
	static ofstream *outputFile;
	static mutex* logMutex;
	static mutex* scrollLogMutex;
	static int index;
};

#endif