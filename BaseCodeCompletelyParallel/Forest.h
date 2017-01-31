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
#include "Dirent.h"

using namespace std;
#define ARCHIVE_FOLDER LOGGERPATH
#define READMEPATH "../../ReadMe.txt"

typedef std::vector<map<PatternType, PListType>>::iterator it_type;
typedef std::map<PatternType, vector<PListType>*>::iterator it_map_list_p_type;
typedef std::map<unsigned int, unsigned int>::iterator it_chunk;

struct LevelPackage
{
	unsigned int currLevel;
	unsigned int threadIndex;
	unsigned int inceptionLevelLOL;
	bool useRAM;
	unsigned int coreIndex;
};

class Forest
{

private:
	PListType memoryCeiling;
	double mostMemoryOverflow;
	double currMemoryOverflow;
	PListType fileID;
	vector<mutex*> gatedMutexes;
	vector<unsigned int> currentLevelVector;
	vector<bool> activeThreads;
	int threadsDispatched;
	int threadsDefuncted;
	vector<future<void>> *threadPool;
	vector<future<void>> *threadPlantSeedPoolHD;
	vector<future<void>> *threadPlantSeedPoolRAM;
	vector<FileReader*> files;
	std::string::size_type sz;
	unsigned int numThreads;
	unsigned int levelToOutput;
	int history;
	PListType minimum, maximum;
	PListType memoryBandwidthMB;
	PListType memoryPerThread;
	unsigned int globalLevel;
	string patternToSearchFor;
	//If /d is in commands then display number of patterns found at each level
	bool displayEachLevelSearch;
	//If /c is in commands then cycle from 1 thread to MAX threads on machine and output best thread scheme
	bool findBestThreadNumber;
	mutex *countMutex;
	mutex *fileIDMutex;
	bool usingMemoryBandwidth;
	unsigned int testIterations;
	bool usingPureRAM;
	bool usingPureHD;
	vector<vector<string>> prevFileNameList;
	vector<vector<string>> newFileNameList;
	double MemoryUsedPriorToThread;
	double MemoryUsageAtInception;
	vector<bool> usedRAM;
	vector<vector<PListType>*>* prevPListArray;
	vector<vector<PListType>*>* globalPListArray;
	PListType eradicatedPatterns;
	vector<PListType> levelRecordings;
	vector<PListType> mostCommonPatternCount;
	vector<string> mostCommonPattern;
	StopWatch initTime;
	bool processingFinished;
	bool processingMSYNCFinished;
	PListType minOccurrence;
	bool writingFlag;
	vector<float> coverage;
	map<unsigned int, unsigned int> chunkIndexToFileChunk;
	vector<string> fileChunks;
	vector<double> statisticsModel;

	void MemoryQuery();
	void MonitorMSYNCThreads();
	void PlantTreeSeedThreadRAM(PListType positionInFile, PListType startPatternIndex, PListType numPatternsToSearch, PListType threadIndex);
	void PlantTreeSeedThreadHD(PListType positionInFile, PListType startPatternIndex, PListType numPatternsToSearch, unsigned int threadNum);
	bool NextLevelTreeSearch(unsigned int level);
	bool NextLevelTreeSearchRecursion(vector<vector<PListType>*>* prevLocalPListArray, vector<vector<PListType>*>* globalLocalPListArray, vector<string>& fileList, LevelPackage& levelInfo);
	void ThreadedLevelTreeSearchRecursionList(vector<vector<PListType>*>* patterns, vector<PListType> patternIndexList, vector<string> fileList, LevelPackage levelInfo);
	bool PredictHardDiskOrRAMProcessing(LevelPackage levelInfo, PListType sizeOfPrevPatternCount);
	void FirstLevelHardDiskProcessing(vector<string>& backupFilenames, unsigned int z);
	string CreateChunkFile(string fileName, vector<vector<PListType>*> leaves, LevelPackage levelInfo);
	string CreateChunkFile(string fileName, TreeHD &leaf, LevelPackage levelInfo);
	void DeleteChunks(vector<string> fileNames, string folderLocation);
	void DeleteChunk(string fileChunkName, string folderLocation);
	void DeleteArchives(vector<string> fileNames, string folderLocation);
	void DeleteArchive(string fileNames, string folderLocation);
	PListType ProcessChunksAndGenerate(vector<string> fileNamesToReOpen, vector<string>& newFileNames, PListType memDivisor, unsigned int threadNum, unsigned int currLevel, unsigned int coreIndex, bool firstLevel = false);
	PListType ProcessChunksAndGenerateLargeFile(vector<string> fileNamesToReOpen, vector<string>& newFileNames, PListType memDivisor, unsigned int threadNum, unsigned int currLevel, bool firstLevel = false);
	bool ProcessHD(LevelPackage& levelInfo, vector<string>& fileList, bool &isThreadDefuncted);
	bool ProcessRAM(vector<vector<PListType>*>* prevLocalPListArray, vector<vector<PListType>*>* globalLocalPListArray, LevelPackage& levelInfo, bool& isThreadDefuncted);
	void PrepDataFirstLevel(bool prediction, vector<vector<string>>& fileList, vector<vector<PListType>*>* prevLocalPListArray = NULL, vector<vector<PListType>*>* globalLocalPListArray = NULL);
	void PrepData(bool prediction, LevelPackage& levelInfo, vector<string>& fileList, vector<vector<PListType>*>* prevLocalPListArray = NULL, vector<vector<PListType>*>* globalLocalPListArray = NULL);
	vector<vector<PListType>> ProcessThreadsWorkLoadRAMFirstLevel(unsigned int threadsToDispatch, vector<vector<PListType>*>* patterns);
	vector<vector<PListType>> ProcessThreadsWorkLoadRAM(unsigned int threadsToDispatch, vector<vector<PListType>*>* patterns);
	vector<vector<string>> ProcessThreadsWorkLoadHD(unsigned int threadsToDispatch, LevelPackage levelInfo, vector<string> prevFileNames);
	void WaitForThreads(vector<unsigned int> localWorkingThreads, vector<future<void>> *localThreadPool, bool recursive = false, unsigned int thread = 0);
	bool DispatchNewThreadsHD(PListType newPatternCount, bool& morePatternsToFind, vector<string> fileList, LevelPackage levelInfo, bool& isThreadDefuncted);
	bool DispatchNewThreadsRAM(PListType newPatternCount, bool& morePatternsToFind, vector<vector<PListType>*>* prevLocalPListArray, LevelPackage levelInfo, bool& isThreadDefuncted);
	void DisplayPatternsFound();
	void DisplayHelpMessage();
	void CommandLineParser(int argc, char **argv);


public:

	int f;

	static bool outlierScans;
	static bool overMemoryCount;
	Forest(int argc, char **argv);
	~Forest();

};
