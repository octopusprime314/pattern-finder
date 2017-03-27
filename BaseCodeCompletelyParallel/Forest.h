#pragma once
#include <iostream>
#include <vector>
#include <map>
#include <future>
#include "TreeRAM.h"
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


typedef std::vector<map<PatternType, PListType>>::iterator it_type;
typedef std::map<PatternType, vector<PListType>*>::iterator it_map_list_p_type;
typedef std::map<unsigned int, unsigned int>::iterator it_chunk;


class Forest
{

private:

	ChunkFactory* chunkFactorio;

	PListType memoryCeiling;
	double mostMemoryOverflow;
	double currMemoryOverflow;
	vector<mutex*> gatedMutexes;
	vector<unsigned int> currentLevelVector;
	vector<bool> activeThreads;
	int threadsDispatched;
	int threadsDefuncted;
	vector<future<void>> *threadPool;
	vector<future<void>> *threadPlantSeedPoolHD;
	vector<future<void>> *threadPlantSeedPoolRAM;
	std::string::size_type sz;
	PListType memoryPerThread;
	unsigned int globalLevel;
	mutex *countMutex;
	vector<vector<string>> prevFileNameList;
	vector<vector<string>> newFileNameList;
	queue<string> filesToBeRemoved;
	mutex filesToBeRemovedLock;
	double MemoryUsedPriorToThread;
	double MemoryUsageAtInception;
	vector<bool> usedRAM;
	vector<vector<PListType>*>* prevPListArray;
	vector<vector<PListType>*>* globalPListArray;
	PListType eradicatedPatterns;
	vector<PListType> levelRecordings;
	vector<PListType> mostCommonPatternCount;
	vector<PListType> mostCommonPatternIndex;
	StopWatch initTime;
	bool processingFinished;
	bool processingMSYNCFinished;
	bool writingFlag;
	vector<float> coverage;
	map<unsigned int, unsigned int> chunkIndexToFileChunk;
	vector<string> fileChunks;
	vector<double> statisticsModel;
	int f;
	vector<double> processingTimes;
	map<PListType, PListType> finalPattern;
	bool firstLevelProcessedHD;

	ConfigurationParams config;


	void MemoryQuery();
	void MonitorMSYNCThreads();
	void PlantTreeSeedThreadRAM(PListType positionInFile, PListType startPatternIndex, PListType numPatternsToSearch, PListType threadIndex);
	void PlantTreeSeedThreadHD(PListType positionInFile, PListType startPatternIndex, PListType numPatternsToSearch, unsigned int threadNum);
	bool NextLevelTreeSearch(unsigned int level);
	bool NextLevelTreeSearchRecursion(vector<vector<PListType>*>* prevLocalPListArray, vector<vector<PListType>*>* globalLocalPListArray, vector<string>& fileList, LevelPackage& levelInfo, PListType patternCount, bool processingRAM = true);
	void ThreadedLevelTreeSearchRecursionList(vector<vector<PListType>*>* patterns, vector<PListType> patternIndexList, vector<string> fileList, LevelPackage levelInfo);
	bool PredictHardDiskOrRAMProcessing(LevelPackage levelInfo, PListType sizeOfPrevPatternCount, PListType sizeOfString = 0);
	void FirstLevelHardDiskProcessing(vector<string>& backupFilenames, unsigned int z);
	PListType ProcessChunksAndGenerate(vector<string> fileNamesToReOpen, vector<string>& newFileNames, PListType memDivisor, unsigned int threadNum, unsigned int currLevel, unsigned int coreIndex, bool firstLevel = false);
	PListType ProcessChunksAndGenerateLargeFile(vector<string> fileNamesToReOpen, vector<string>& newFileNames, PListType memDivisor, unsigned int threadNum, unsigned int currLevel, bool firstLevel = false);
	PListType ProcessHD(LevelPackage& levelInfo, vector<string>& fileList, bool &isThreadDefuncted);
	PListType ProcessRAM(vector<vector<PListType>*>* prevLocalPListArray, vector<vector<PListType>*>* globalLocalPListArray, LevelPackage& levelInfo, bool& isThreadDefuncted);
	void PrepDataFirstLevel(bool prediction, vector<vector<string>>& fileList, vector<vector<PListType>*>* prevLocalPListArray = NULL, vector<vector<PListType>*>* globalLocalPListArray = NULL);
	void PrepData(bool prediction, LevelPackage& levelInfo, vector<string>& fileList, vector<vector<PListType>*>* prevLocalPListArray = NULL, vector<vector<PListType>*>* globalLocalPListArray = NULL);
	vector<vector<PListType>> ProcessThreadsWorkLoadRAMFirstLevel(unsigned int threadsToDispatch, vector<vector<PListType>*>* patterns);
	vector<vector<PListType>> ProcessThreadsWorkLoadRAM(unsigned int threadsToDispatch, vector<vector<PListType>*>* patterns);
	vector<vector<string>> ProcessThreadsWorkLoadHD(unsigned int threadsToDispatch, LevelPackage levelInfo, vector<string> prevFileNames);
	void WaitForThreads(vector<unsigned int> localWorkingThreads, vector<future<void>> *localThreadPool, bool recursive = false, unsigned int thread = 0);
	bool DispatchNewThreadsHD(PListType newPatternCount, bool& morePatternsToFind, vector<string> fileList, LevelPackage levelInfo, bool& isThreadDefuncted);
	bool DispatchNewThreadsRAM(PListType newPatternCount, PListType& morePatternsToFind, vector<PListType> &linearList, vector<PListType> &pListLengths, LevelPackage levelInfo, bool& isThreadDefuncted);


public:

	static bool outlierScans;
	static bool overMemoryCount;
	Forest(int argc, char **argv);
	~Forest();

};
