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

using namespace std;
#define ARCHIVE_FOLDER "../Log/"
#define READMEPATH "../ReadMe.txt"
#define PIPELINE_LENGTH 4
#define NEW_PROCESS 0
typedef std::vector<map<PatternType, PListType>>::iterator it_type;
typedef std::map<PatternType, vector<PListType>*>::iterator it_map_list_p_type;
typedef std::vector<pair<PatternType, vector<PListType>>*>::iterator it_map_list_p_type_test;
typedef std::map<unsigned int, unsigned int>::iterator it_chunk;
typedef std::list<PatternType>::iterator string_it_type;
typedef std::list<pair<PListType, uint8_t>>::iterator pair_it_type;
typedef std::unordered_map<size_t, PListType>::iterator junk_it;
struct LevelPackage
{
	unsigned int currLevel;
	unsigned int threadIndex;
	unsigned int inceptionLevelLOL;
	bool useRAM;
	unsigned int coreIndex;
};

struct Quad
{
	PListType index;
	PListType pListArrayIndex;
	PListType order;
	uint8_t patternId;
};

struct Tuple
{
	PListType index;
	PListType pListArrayIndex;
	uint8_t patternId;
};

struct Duple
{
	PListType index;
	PListType pListArrayIndex;
};

struct CopyCat
{
	PListType index;
	PListType pListArrayIndex;
};

struct sort_quad {
    bool operator()(const Quad &left, Quad &right) {
        return left.index < right.index;
    }
};

struct sortier {
    bool operator()(const pair<PListType, PListType> &left, const pair<PListType, PListType> &right) {
        return left.first < right.first;
    }
};

struct sort_duple {
    bool operator()(const Duple &left, Duple &right) {
        return left.index < right.index;
    }
};


struct sort_tuple {
    bool operator()(const Tuple &left, const Tuple &right) {
        return left.index < right.index;
    }
};

struct sort_tuple2 {
    bool operator()(const Tuple &left, Tuple &right) {
        return left.pListArrayIndex < right.pListArrayIndex;
    }
};

struct sort_pred {
    bool operator()(const std::pair<PListType, uint8_t> &left, const std::pair<PListType, uint8_t> &right) {
        return left.second < right.second;
    }
};

struct sort_pred2 {
    bool operator()(const pair<PListType, PListType> &left, const pair<PListType, PListType> &right) {
        return left.first < right.first;
    }
};

struct sort_pred3 {
    bool operator()(const std::pair<PListType, array<uint8_t,2>> &left, const std::pair<PListType, array<uint8_t,2>> &right) {
        return left.second[0] < right.second[0];
    }
};

struct sort_pred4 {
    bool operator()(const std::pair<PListType, PListType> &left, const std::pair<PListType, PListType> &right) {
        return left.first < right.first;
    }
};




static int level2Counts[256] = {0};
static int resized[256] = {0};
static int totalWrites = 0;

class Forest
{
private:
	PListType memoryCeiling;
	double mostMemoryOverflow;
	double currMemoryOverflow;
	PListType fileID;
	vector<mutex*> gatedMutexes;
	vector<mutex*> resizeMutexes;
	vector<unsigned int> currentLevelVector;
	vector<bool> activeThreads;
	int threadsDispatched;
	int threadsDefuncted;
	vector<future<void>> *threadPool;
	vector<future<void>> *threadPlantSeedPoolHD;
	vector<future<void>> *threadPlantSeedPoolRAM;
	FileReader *file;
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
	PListType startingLevel;
	vector<vector<string>> prevFileNameList;
	vector<vector<string>> newFileNameList;
	double MemoryUsedPriorToThread;
	double MemoryUsageAtInception;
	vector<bool> usedRAM;
	PListType sizeOfPreviousLevelMB;
	vector<vector<PListType>*>* prevPListArray;
	vector<vector<PListType>*>* globalPListArray;
	
	vector<vector<vector<PListType>*>*>* localPrevPListArrayTriplet;
	vector<vector<vector<PListType>*>*>* localGlobalPListArrayTriplet;

	PListType eradicatedPatterns;
	vector<PListType> levelRecordings;
	vector<PListType> mostCommonPatternCount;
	vector<string> mostCommonPattern;
	StopWatch initTime;
	bool globalUsingRAM;
	
	bool processingFinished;
	bool processingMSYNCFinished;
	PListType minOccurrence;

	bool writingFlag;
	
	vector<float> coverage;
	//-1 indicates the thread is not using any file chunk at the moment
	vector<signed long> usingFileChunk;
	vector<signed long> chunkOrigin;
	//thread using which threads data ie it could be its own or another thread's file data
	map<unsigned int, unsigned int> chunksBeingUsed;

	map<unsigned int, unsigned int> chunkIndexToFileChunk;

	vector<string> fileChunks;

	vector<unsigned int> availableCores;

	vector<double> statisticsModel;

	void MemoryQuery();
	void MonitorMSYNCThreads();

	void ProcessChunksIntoUnifiedArray(PListType startPatternIndex, PListType numPatternsToSearch);

	void PlantTreeSeedThreadRAM(PListType positionInFile, PListType startPatternIndex, PListType numPatternsToSearch, PListType threadIndex);
	void PlantTreeSeedThreadHD(PListType positionInFile, PListType startPatternIndex, PListType numPatternsToSearch, unsigned int threadNum);
	
	bool NextLevelTreeSearch(unsigned int level);
	bool NextLevelTreeSearchRecursion(vector<vector<PListType>*>* prevLocalPListArray, vector<vector<PListType>*>* globalLocalPListArray, vector<string>& fileList, LevelPackage& levelInfo);
	
	void ThreadedLevelTreeSearchRecursionList(vector<vector<PListType>*>* patterns, vector<PListType> patternIndexList, vector<string> fileList, LevelPackage levelInfo);
	
	bool PredictHardDiskOrRAMProcessing(LevelPackage levelInfo, PListType sizeOfPrevPatternCount);
	void FirstLevelHardDiskProcessing(vector<string>& backupFilenames, unsigned int z);

	string CreateChunkFile(string fileName, TreeHD& leaf, LevelPackage levelInfo);
	void DeleteChunks(vector<string> fileNames, string folderLocation);
	void DeleteChunk(string fileChunkName, string folderLocation);
	void DeleteArchives(vector<string> fileNames, string folderLocation);
	void DeleteArchive(string fileNames, string folderLocation);

	PListType ProcessChunksAndGenerate(vector<string> fileNamesToReOpen, vector<string>& newFileNames, PListType memDivisor, unsigned int threadNum, unsigned int currLevel, unsigned int coreIndex, bool firstLevel = false);
	PListType ProcessChunksAndGenerateLargeFile(vector<string> fileNamesToReOpen, vector<string>& newFileNames, PListType memDivisor, unsigned int threadNum, unsigned int currLevel, bool firstLevel = false);

	bool ProcessHD(LevelPackage& levelInfo, vector<string>& fileList, bool &isThreadDefuncted);
	//bool ProcessRAM(vector<Duple>& prevLocalPListArray, vector<Duple>& globalLocalPListArray, LevelPackage& levelInfo, bool &isThreadDefuncted);

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
	static bool outlierScans;
	static bool overMemoryCount;
	Forest(int argc, char **argv);
	~Forest();

};
