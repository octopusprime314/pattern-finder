#pragma once
#include <iostream>
#include <vector>
#include <map>
#include <future>
#include "TreeHD.h"
#include "FileReader.h"
#include <sstream>
#include <ctime>
#include <iomanip>
#include "PListArchive.h"
#include "StopWatch.h"
#include <array>
#include <queue>
#include "ChunkFactory.h"
#include "ProcessorConfig.h"
#include "ProcessorStats.h"

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

private:

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
	vector<bool> usedRAM;
	vector<bool> activeThreads;
	vector<double> processingTimes;
	
	void MemoryQuery();
	void PlantTreeSeedThreadRAM(PListType positionInFile, PListType startPatternIndex, PListType numPatternsToSearch, PListType threadIndex);
	void PlantTreeSeedThreadHD(PListType positionInFile, PListType startPatternIndex, PListType numPatternsToSearch, unsigned int threadNum);
	bool NextLevelTreeSearch(unsigned int level);
	bool NextLevelTreeSearchRecursion(vector<vector<PListType>*>* prevLocalPListArray, vector<vector<PListType>*>* globalLocalPListArray, vector<string>& fileList, LevelPackage& levelInfo, PListType patternCount, bool processingRAM = true);
	void ThreadedLevelTreeSearchRecursionList(vector<vector<PListType>*>* patterns, vector<PListType> patternIndexList, vector<string> fileList, LevelPackage levelInfo);
	bool PredictHardDiskOrRAMProcessing(LevelPackage levelInfo, PListType sizeOfPrevPatternCount, PListType sizeOfString = 0);
	PListType ProcessChunksAndGenerate(vector<string> fileNamesToReOpen, vector<string>& newFileNames, PListType memDivisor, unsigned int threadNum, unsigned int currLevel, unsigned int coreIndex, bool firstLevel = false);
	PListType ProcessChunksAndGenerateLargeFile(vector<string> fileNamesToReOpen, vector<string>& newFileNames, PListType memDivisor, unsigned int threadNum, unsigned int currLevel, bool firstLevel = false);
	PListType ProcessHD(LevelPackage& levelInfo, vector<string>& fileList, bool &isThreadDefuncted);
	PListType ProcessRAM(vector<vector<PListType>*>* prevLocalPListArray, vector<vector<PListType>*>* globalLocalPListArray, LevelPackage& levelInfo, bool& isThreadDefuncted);
	void PrepDataFirstLevel(bool prediction, vector<vector<string>>& fileList, vector<vector<PListType>*>* prevLocalPListArray = NULL);
	void PrepData(bool prediction, LevelPackage& levelInfo, vector<string>& fileList, vector<vector<PListType>*>* prevLocalPListArray = NULL, vector<vector<PListType>*>* globalLocalPListArray = NULL);
	vector<vector<PListType>> ProcessThreadsWorkLoadRAMFirstLevel(unsigned int threadsToDispatch, vector<vector<PListType>*>* patterns);
	vector<vector<PListType>> ProcessThreadsWorkLoadRAM(unsigned int threadsToDispatch, vector<vector<PListType>*>* patterns);
	vector<vector<string>> ProcessThreadsWorkLoadHD(unsigned int threadsToDispatch, LevelPackage levelInfo, vector<string> prevFileNames);
	void WaitForThreads(vector<unsigned int> localWorkingThreads, vector<future<void>> *localThreadPool, bool recursive = false, unsigned int thread = 0);
	bool DispatchNewThreadsHD(PListType newPatternCount, bool& morePatternsToFind, vector<string> fileList, LevelPackage levelInfo, bool& isThreadDefuncted);
	bool DispatchNewThreadsRAM(PListType newPatternCount, PListType& morePatternsToFind, vector<PListType> &linearList, vector<PListType> &pListLengths, LevelPackage levelInfo, bool& isThreadDefuncted);


public:

	static bool overMemoryCount;
	Forest(int argc, char **argv);
	~Forest();

};
