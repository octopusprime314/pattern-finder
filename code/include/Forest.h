/** @file Forest.h
*  @brief Contains algorithms to process patterns
*
*  Algorithms that process patterns within a file using
*  either excusively the hard drive or ram but can also
*  manage switching to hard drive or ram processing at any level
*  for speed improvements...this class needs to be refactored
*  to have a base class processor with derived classes called
*  hdprocessor and ramprocessor
*
*  @author Peter J. Morley (pmorley)
*/

#pragma once
#include <iostream>
#include <vector>
#include <map>
#include <future>
#include "TreeHD.h"
#include "FileReader.h"
#include "PListArchive.h"
#include "StopWatch.h"
#include "ChunkFactory.h"
#include "ProcessorConfig.h"
#include "ProcessorStats.h"
#include "SysMemProc.h"
#include "DiskProc.h"

#if defined(_WIN64) || defined(_WIN32)
#include "Dirent.h"
#elif defined(__linux__)
#include "sys/stat.h"
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#endif

using namespace std;

class Forest
{


public:

    /** @brief Constructor.
    */
    Forest(int argc, char **argv);

    /** @brief Destructor.
    */
    ~Forest();

    /** @brief Polls the amount of memory the program has allocated
    *
    *  If the memory consumed by the process is reaching the capacity
    *  of the system's ram then the program will quit to prevent virtual
    *  page thrashing and thus system slow down
    *
    *  @return void
    */
    void MemoryQuery();

    /** @brief Processes the first level using ram only
    *
    *  The first level is unique because every thread can have patterns
    *  that the other threads may also have so they have must be processed
    *  differently and compiled together at the end of processing the first level
    *
    *  @param positionInFile offset position in the file used by startPatternIndex
    *  @param startPatternIndex index in the file to begin processing
    *  @param numPatternsToSearch number of indexes within the file to search for patterns
    *  @param threadIndex the current thread being used to process this chunk of the file
    *  @return void
    */
    void PlantTreeSeedThreadRAM(PListType positionInFile, PListType startPatternIndex, PListType numPatternsToSearch, PListType threadIndex);

    /** @brief Processes the first level using hard disk and ram
    *
    *  The first level is unique because every thread can have patterns
    *  that the other threads may also have so they have must be processed
    *  differently and compiled together at the end of processing the first level
    *
    *  @param positionInFile offset position in the file used by startPatternIndex
    *  @param startPatternIndex index in the file to begin processing
    *  @param numPatternsToSearch number of indexes within the file to search for patterns
    *  @param threadNum the current thread being used to process this chunk of the file
    *  @return void
    */
    void PlantTreeSeedThreadHD(PListType positionInFile, PListType startPatternIndex, PListType numPatternsToSearch, unsigned int threadNum);
  
    /** @brief Processes all of the first level partial pattern data files when using hd processing
    *
    *  Compiles all of the first level partial pattern files and creates one cohesive pattern collection
    *
    *  @param fileNamesToReOpen partial pattern files to be processed into one big pattern collection
    *  @param newFileNames new files generated containing complete pattern information
    *  @param memDivisor memory that can be used for this thread
    *  @param threadNum thread used for this process
    *  @param currLevel current level being processed
    *  @param firstLevel indicates whether this is first level processing or not
    *  @return PListType returns number of patterns found
    */
    PListType ProcessChunksAndGenerateLargeFile(vector<string> fileNamesToReOpen, vector<string>& newFileNames, PListType memDivisor, unsigned int threadNum, unsigned int currLevel, bool firstLevel = false);

    /** @brief Switches pattern data for either ram or hd processing on the first level
    *
    *  If the prediction determines that the upcoming level can be processed
    *  using ram and the previous level was processed with the hd then a data conversion
    *  must happen.  The reverse data conversion must happen if ram was processed
    *  previously and the current level needs to be processed using the hard disk
    *
    *  @param prediction boolean indicating the current level will be processed with ram or hd
    *  @param fileList hark disk processing data
    *  @param prevLocalPListArray ram processing data
    *  @return void
    */
    void PrepDataFirstLevel(bool prediction, vector<vector<string>>& fileList, vector<vector<PListType>*>* prevLocalPListArray = nullptr);

    /** @brief Switches pattern data for either ram or hd processing on levels after the first
    *
    *  If the prediction determines that the upcoming level can be processed
    *  using ram and the previous level was processed with the hd then a data conversion
    *  must happen.  The reverse data conversion must happen if ram was processed
    *  previously and the current level needs to be processed using the hard disk
    *
    *  @param prediction boolean indicating the current level will be processed with ram or hd
    *  @param levelInfo current level processing information
    *  @param fileList hark disk processing data
    *  @param prevLocalPListArray ram processing data
    *  @return void
    */
    void PrepData(bool prediction, LevelPackage& levelInfo, vector<string>& fileList, vector<vector<PListType>*>* prevLocalPListArray = nullptr);

    /** @brief Evenly distributes ram vector data among the threads to be dispatched for the first level
    *
    *  Distributes pattern vector information among the threads for the first level.
    *
    *  @param threadsToDispatch number of threads that need distributed pattern data
    *  @param patterns vector containing pattern data
    *  @return vector<vector<PListType>> returns distributed ram vector pattern loads
    */
    vector<vector<PListType>> ProcessThreadsWorkLoadRAMFirstLevel(unsigned int threadsToDispatch, vector<vector<PListType>*>* patterns);

    /** @brief Evenly distributes hd pattern file data among the threads to be dispatched
    *
    *  Distributes file information among the threads if there are the same number of
    *  files as there are threads.  If there are not enough files to give each thread
    *  a file then the files must be read in and distributed so each thread gets one
    *  file which is time consuming.
    *
    *  @param threadsToDispatch number of threads that need distributed pattern data
    *  @param levelInfo current level processing information
    *  @param prevFileNames files that are to be distributed
    *  @return vector<vector<string>> returns distributed hd file pattern loads
    */
    vector<vector<string>> ProcessThreadsWorkLoadHD(unsigned int threadsToDispatch, LevelPackage levelInfo, vector<string> prevFileNames);

    /** @brief Keeps track of the state of each processing thread
    *
    *  Monitors threads and releases processing holds on a thread when a thread has
    *  finished processing.  This allows other threads that still have work to do
    *  to utilize an extra thread for their processing.  It is a simplified
    *  implementation of a thread pool.
    *
    *  @param localWorkingThreads indexes into the localWorkingThreads that need to be monitored
    *  @param localThreadPool vector containing threads to monitor
    *  @param recursive indicating if these threads were dispatched recursively or not
    *  @param thread current thread being used
    *  @return void
    */
    void WaitForThreads(vector<unsigned int> localWorkingThreads, vector<future<void>> *localThreadPool, bool recursive = false, unsigned int thread = 0);


private:

	void inteTochar(FileReader * files);

    //Indicates whether memory usage is over the limit
    static bool overMemoryCount;

    ChunkFactory* chunkFactorio;
    ConfigurationParams config;
    ProcessorStats stats;
    StopWatch initTime;

    //Memory management
    double mostMemoryOverflow;
    double currMemoryOverflow;
    double MemoryUsedPriorToThread;

    //Thread management
    mutex *countMutex;
    int threadsDefuncted;
    int threadsDispatched;

    vector<future<void>> *threadPool;
  
    //Random flags
    bool writingFlag;
    bool processingFinished;
    bool firstLevelProcessedHD;

    //File handling
    vector<string> fileChunks;
    vector<vector<string>> newFileNameList;
    map<PListType, PListType> finalPattern;
    vector<vector<string>> prevFileNameList;
    map<unsigned int, unsigned int> chunkIndexToFileChunk;

    //Global data collection
    vector<vector<PListType>*>* prevPListArray;

    //File statistics
    vector<bool> activeThreads;
    vector<double> processingTimes;


};