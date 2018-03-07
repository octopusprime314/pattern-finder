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
#include <lexical_cast.hpp>
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

	/** @brief Preps pattern data for processing level 2 to infinity
	 *  
	 *  Preps data distribution for threaded processing
	 *
	 *  @param level level to begin processing
	 *  @return bool indicates if more processing is necessary
	 */
	bool NextLevelTreeSearch(unsigned int level);

	/** @brief Takes in pattern data and creates new threads to dispatch
	 *  
	 *  Recursively generates new threads for processing the file data
	 *
	 *  @param patterns set of patterns to process from 
	 *  @param patternIndexList which patterns to search for within the large pattern set in patterns
	 *  @param fileList hark disk processing data
	 *  @param levelInfo current level processing information
	 *  @return void
	 */
	void ThreadedLevelTreeSearchRecursionList(vector<vector<PListType>*>* patterns, vector<PListType> patternIndexList, vector<string> fileList, LevelPackage levelInfo);

	/** @brief Algorithm that computes whether the next level uses hd or ram
	 *  
	 *  Factors in the previous level's mode of processing and the current level's
	 *  intended mode of process and computes the resources needed to process within
	 *  the computer's limited ram
	 *
	 *  @param levelInfo current level processing information
	 *  @param sizeOfPrevPatternCount number of patterns found from the previous level
	 *  @param sizeOfString size of the patterns for the level
	 *  @return bool indicating whether ram or hd is used, true is hd, false is ram
	 */
	bool PredictHardDiskOrRAMProcessing(LevelPackage levelInfo, PListType sizeOfPrevPatternCount, PListType sizeOfString = 0);

	/** @brief Processes all of the partial pattern data files when using hd processing
	 *  
	 *  Compiles all of the partial pattern files and creates one cohesive pattern collection
	 *
	 *  @param fileNamesToReOpen partial pattern files to be processed into one big pattern collection
	 *  @param newFileNames new files generated containing complete pattern information
	 *  @param memDivisor memory that can be used for this thread
	 *  @param threadNum thread used for this process
	 *  @param currLevel current level being processed
	 *  @param coreIndex core being used
	 *  @param firstLevel indicates whether this is first level processing or not
	 *  @return PListType returns number of patterns found
	 */
	PListType ProcessChunksAndGenerate(vector<string> fileNamesToReOpen, vector<string>& newFileNames, PListType memDivisor, unsigned int threadNum, unsigned int currLevel, unsigned int coreIndex, bool firstLevel = false);

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
	
	/** @brief Processes patterns using a combination of ram and hard disk
	 *  
	 *  Hard disk processing which is much slower than using ram
	 *
	 *  @param levelInfo current level processing information
	 *  @param fileList hark disk processing data
	 *  @param isThreadDefuncted indicates if the thread is deactivated
	 *  and there has been recursively spawned threads to improve processing
	 *  @return PListType indicating the number of patterns found for the level
	 */
	PListType ProcessHD(LevelPackage& levelInfo, vector<string>& fileList, bool &isThreadDefuncted);

	/** @brief Processes patterns using ram 
	 *  
	 *  RAM processing is much faster than HD processing
	 *
	 *  @param prevLocalPListArray previous level ram processing data 
	 *  @param globalLocalPListArray current level ram processing data 
	 *  @param levelInfo current level processing information
	 *  @param isThreadDefuncted indicates if the thread is deactivated
	 *  and there has been recursively spawned threads to improve processing
	 *  @return PListType indicating the number of patterns found for the level
	 */
	PListType ProcessRAM(vector<vector<PListType>*>* prevLocalPListArray, vector<vector<PListType>*>* globalLocalPListArray, LevelPackage& levelInfo, bool& isThreadDefuncted);

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
	void PrepDataFirstLevel(bool prediction, vector<vector<string>>& fileList, vector<vector<PListType>*>* prevLocalPListArray = NULL);

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
	void PrepData(bool prediction, LevelPackage& levelInfo, vector<string>& fileList, vector<vector<PListType>*>* prevLocalPListArray = NULL);

	/** @brief Evenly distributes ram vector data among the threads to be dispatched for the first level
	 *  
	 *  Distributes pattern vector information among the threads for the first level.  
	 *
	 *  @param threadsToDispatch number of threads that need distributed pattern data
	 *  @param patterns vector containing pattern data
	 *  @return vector<vector<PListType>> returns distributed ram vector pattern loads
	 */
	vector<vector<PListType>> ProcessThreadsWorkLoadRAMFirstLevel(unsigned int threadsToDispatch, vector<vector<PListType>*>* patterns);

	/** @brief Evenly distributes ram vector data among the threads to be dispatched
	 *  
	 *  Distributes pattern vector information among the threads.  
	 *
	 *  @param threadsToDispatch number of threads that need distributed pattern data
	 *  @param patterns vector containing pattern data
	 *  @return vector<vector<PListType>> returns distributed ram vector pattern loads
	 */
	vector<vector<PListType>> ProcessThreadsWorkLoadRAM(unsigned int threadsToDispatch, vector<vector<PListType>*>* patterns);
	
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

	/** @brief Evenly distributes pattern data among multiple threads for hd processing
	 *  
	 *  This algorithm looks for available processing threads and if there are enough
	 *  patterns to distribute and available threads then new threads are recursively
	 *  dispatched and the current thread just listens and waits for the dispatched
	 *  threads to finish processing.
	 *
	 *  @param newPatternCount number of patterns found in the current level search
	 *  @param morePatternsToFind boolean indicating patterns have been found or not
	 *  @param fileList hark disk processing data
	 *  @param levelInfo current level processing information
	 *  @param isThreadDefuncted indicates if the thread is deactivated
	 *  and there has been recursively spawned threads to improve processing
	 *  @return bool indicating the threads have been dispatched or not
	 */
	bool DispatchNewThreadsHD(PListType newPatternCount, bool& morePatternsToFind, vector<string> fileList, LevelPackage levelInfo, bool& isThreadDefuncted);

	/** @brief Evenly distributes pattern data among multiple threads for ram processing
	 *  
	 *  This algorithm looks for available processing threads and if there are enough
	 *  patterns to distribute and available threads then new threads are recursively
	 *  dispatched and the current thread just listens and waits for the dispatched
	 *  threads to finish processing.
	 *
	 *  @param newPatternCount number of patterns found in the current level search
	 *  @param morePatternsToFind PListType indicating the number of patterns found at this level
	 *  @param linearList linear vector of all pattern indexes
	 *  @param pListLengths the lengths of each pattern index list for accessing linearList
	 *  @param levelInfo current level processing information
	 *  @param isThreadDefuncted indicates if the thread is deactivated
	 *  and there has been recursively spawned threads to improve processing
	 *  @return bool indicating the threads have been dispatched or not
	 */
	bool DispatchNewThreadsRAM(PListType newPatternCount, PListType& morePatternsToFind, vector<PListType> &linearList, vector<PListType> &pListLengths, LevelPackage levelInfo, bool& isThreadDefuncted);

	

private:

	//Indicates whether memory usage is over the limit
	static bool overMemoryCount;

	ChunkFactory* chunkFactorio;
	ConfigurationParams config;
	ProcessorStats stats;
	StopWatch initTime;

	//Memory management
	PListType memoryCeiling;
	double mostMemoryOverflow;
	double currMemoryOverflow;
	double MemoryUsageAtInception;
	double MemoryUsedPriorToThread;

	//Thread management
	mutex *countMutex;
	int threadsDefuncted;
	int threadsDispatched;
	PListType memoryPerThread;
	vector<future<void>> *threadPool;

	//Random flags
	int f;
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
