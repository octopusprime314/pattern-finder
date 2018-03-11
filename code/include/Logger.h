/** @file Logger.h
 *  @brief Generates program logging information and csv pattern data
 *
 *  Used for general program logging and generating pattern data output
 *
 *  @author Peter J. Morley (pmorley)
 */
#pragma once
#include <iostream>
#include <sstream>
#include <fstream>
#include <mutex>
#include "TypeDefines.h"
#include <vector>
#include <map>
using namespace std;

/** Paths to logger and csv files are different based on OS */
#if defined(_WIN64) || defined(_WIN32)
const string LOGGERPATH = "../../log/";
const string CSVPATH = "../../runs/";
#elif defined(__linux__)
const string LOGGERPATH = "../log/";
const string CSVPATH = "../runs/";
#endif

const bool cmdEnabled = true;
const bool disableLogging = false;

class Logger
{
public:

    //This is terrible logging because it logs the number of args - 1
    //I needed a way to pass though variadic arguments and it turned out poorly
    template<typename T, typename... Args>
    static void WriteLog(T streamable, Args... args) {
        
        if (!verbosity || disableLogging) {
            return;
        }
        std::stringstream stream;
        writeLog(stream, streamable, args...);
    }

    template<typename T, typename... Args>
    static void writeLog(std::stringstream& stream, T streamable, Args... args) {
       
        stream << streamable;
        writeLog(stream, args...);
        auto buffer = stream.str();
        dumpLog(buffer);
    }

    template<typename T>
    static void writeLog(std::stringstream& stream, T streamable) {
        stream << streamable << std::endl;
    }

    static void dumpLog(const std::string& buffer);

	/** @brief Flushes any logging data to file and clears the logging buffer
	 *
	 *  Resets log buffer
	 * 
	 *  @return void
	 */
	static void ClearLog();

	/** @brief Closes log file by closing handle
	 *
	 *  Closes file
	 *
	 *  @return void
	 */
	static void CloseLog();

	/** @brief Gets the formatted time in am/pm format
	 *
	 *  Generate am/pm time in string format
	 *
	 *  @return string am/pm time
	 */
	static string GetFormattedTime();

	/** @brief Gets the time in military format
	 *
	 *  Generate military time in string format
	 *
	 *  @return string military time
	 */
	static string GetTime();

	/** sets logging on or off */
	static int verbosity;

	/** @brief Generates a csv file with the time taken to process a set of files
	 *
	 *  Processes a large data set and records the time taken and size of a processed file
	 *  and is written to a csv formatted file to eventually be used by a matlab script
	 *  to find large patterns in a file set.  If a file is small and it takes a long
	 *  time to process then it is typically an indicator of a large pattern in a file.
	 *  The matlab script to process this file is called ProcessTimeVsFileSize.m in
	 *  the Matlab folder.
	 *
	 *  @param processTimes vector containing processing times for each file
	 *  @param fileSizes vector containing the size in bytes of each file
	 *  @return void
	 */
	static void generateTimeVsFileSizeCSV(vector<double> processTimes, vector<PListType> fileSizes);

	/** @brief Generates a csv file with the most common patterns in a large data set
	 *
	 *  Processes a large data set and records the most common patterns found for
	 *  all processed files and is written to a csv formatted file.
	 *
	 *  @param finalPattern map containing counts and the most common pattern
	 *  found in a large data set
	 *
	 *  @return void
	 */
	static void generateFinalPatternVsCount(map<PListType, PListType> finalPattern);

	/** @brief Generates a csv file with throughput improvements associated with threading
	 *
	 *  Processes a single file and records the times taken to process the file with
	 *  a designated amount of threads.  Typically the -c argument tells the program to
	 *  process the file first with one thread and then double the thread count each run until
	 *  the thread count reaches the number of core's on the resident computer.  The matlab
	 *  script used to process and graph this data is called DRAMVsHardDiskPerformance.m
	 *
	 *  @param threadMap map containing times taken to process a file with a number of threads
	 *  used to process
	 *  @return void
	 */
	static void generateThreadsVsThroughput(vector<map<int, double>> threadMap);

	/** @brief Generates a csv file with partial pattern data used when doing split processing
	 *
	 *  Processes a portion of the file and generates pattern data to be compiled together with
	 *  other Pattern Finder processes that generate other portion data.  This csv generated data
	 *  is used in the PatternProcessor.m matlab file when using the python splitFileForProcessing.py
	 *  script.
	 *
	 *  @param file contents of file being processed
	 *  @param patternIndexes vector of indexes where the most common patterns are located in the file
	 *  @param patternCounts vector of how many instances the pattern is found in the file
	 *  @return void
	 */
	static void fillPatternData(const string &file, const vector<PListType> &patternIndexes, const vector<PListType> &patternCounts);

	/** @brief Generates coverage information when comparing overlapping versus non overlapping searches
	 *
	 *  Processes a file in either overlapping or non overlapping mode and then compares the csv files
	 *  patterns using the matlab script called Overlapping_NonOverlappingComparison.m script.
	 *  The overlapping csv file contains 100% accurate pattern information and the non-overlapping csv
	 *  may or may not contain all the correct pattern information.  The matlab script makes sure the file
	 *  is acceptable to run using the non overlapping search which is much faster if the file contains
	 *  very large patterns
	 *
	 *  @param coverage records the coverage of the most common patterns
	 *  @return void
	 */
	static void fileCoverageCSV(const vector<float>& coverage);

	static ofstream* patternOutputFile;

private:
	/** Streams used to write and read to files */
	static string stringBuffer;
	static ofstream* outputFile;
	static ofstream* patternDataFile;
	static ofstream* coverageFile;

	/** Mutexes used for logging */
	static mutex* logMutex;

	/** Get the Process ID of the current process */
	static string GetPID();
};