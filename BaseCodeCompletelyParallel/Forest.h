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
using namespace std;
#define ARCHIVE_FOLDER "../Log/"
#define READMEPATH "../ReadMe.txt"

typedef std::vector<map<PatternType, PListType>>::iterator it_type;
typedef std::map<PatternType, vector<PListType>*>::iterator it_map_list_p_type;
typedef std::map<unsigned int, unsigned int>::iterator it_chunk;
typedef std::list<PatternType>::iterator string_it_type;
struct LevelPackage
{
	unsigned int currLevel;
	unsigned int threadIndex;
	unsigned int inceptionLevelLOL;
	unsigned int useRAM;
	PListType previousPatternsFound;
};

class Forest
{
private:
	PListType memoryCeiling;
	double mostMemoryOverflow;
	double currMemoryOverflow;
	PListType fileID;
	vector<mutex*> gatedMutexes;
	vector<int> currentLevelVector;
	vector<bool> activeThreads;
	PListType threadsDispatched;
	PListType threadsDefuncted;
	vector<future<void>> *threadPool;
	vector<future<void>> *threadPlantSeedPoolHD;
	vector<future<TreeRAM*>> *threadPlantSeedPoolRAM;
	FileReader *file;
	std::string::size_type sz;
	unsigned int numThreads;
	unsigned int levelToOutput;
	bool history;
	PListType minimum, maximum;
	PListType memoryBandwidthMB;
	PListType memoryPerThread;
	PListType globalLevel;
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
	PListType MemoryUsedPriorToThread;
	PListType MemoryUsageAtInception;
	vector<bool> usedRAM;
	PListType sizeOfPreviousLevelMB;
	vector<vector<PListType>*>* prevPListArray;
	vector<vector<PListType>*>* globalPListArray;
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

	void MemoryQuery();
	void MonitorMSYNCThreads();

	TreeRAM* PlantTreeSeedThreadRAM(PListType positionInFile, PListType startPatternIndex, PListType numPatternsToSearch);
	void PlantTreeSeedThreadHD(PListType positionInFile, PListType startPatternIndex, PListType numPatternsToSearch, unsigned int threadNum);
	
	bool NextLevelTreeSearch(PListType level);
	bool NextLevelTreeSearchRecursion(vector<vector<PListType>*>* prevLocalPListArray, vector<vector<PListType>*>* globalLocalPListArray, vector<string>& fileList, LevelPackage& levelInfo);
	
	void ThreadedLevelTreeSearchRecursionList(vector<vector<PListType>*>* patterns, vector<PListType> patternIndexList, vector<string> fileList, LevelPackage levelInfo);
	
	bool PredictHardDiskOrRAMProcessing(LevelPackage levelInfo, PListType sizeOfPrevPatternCount);
	void FirstLevelHardDiskProcessing(vector<string>& backupFilenames, unsigned int z);
	void FirstLevelRAMProcessing();

	string CreateChunkFile(string fileName, TreeHD& leaf, unsigned int threadNum, PListType currLevel);
	void DeleteChunks(vector<string> fileNames, string folderLocation);
	void DeleteChunk(string fileChunkName, string folderLocation);
	void DeleteArchives(vector<string> fileNames, string folderLocation);
	void DeleteArchive(string fileNames, string folderLocation);

	PListType ProcessChunksAndGenerate(vector<string> fileNamesToReOpen, vector<string>& newFileNames, PListType memDivisor, unsigned int threadNum, unsigned int currLevel, bool firstLevel = false);
	PListType ProcessChunksAndGenerateLargeFile(vector<string> fileNamesToReOpen, vector<string>& newFileNames, PListType memDivisor, unsigned int threadNum, unsigned int currLevel, bool firstLevel = false);

	bool ProcessHD(LevelPackage& levelInfo, vector<string>& fileList, bool &isThreadDefuncted);
	bool ProcessRAM(vector<vector<PListType>*>* prevLocalPListArray, vector<vector<PListType>*>* globalLocalPListArray, LevelPackage& levelInfo, bool& isThreadDefuncted);
	void PrepDataFirstLevel(bool prediction, vector<vector<string>>& fileList, vector<vector<PListType>*>* prevLocalPListArray = NULL, vector<vector<PListType>*>* globalLocalPListArray = NULL);
	void PrepData(bool prediction, LevelPackage& levelInfo, vector<string>& fileList, vector<vector<PListType>*>* prevLocalPListArray = NULL, vector<vector<PListType>*>* globalLocalPListArray = NULL);

	vector<vector<PListType>> ProcessThreadsWorkLoadRAM(unsigned int threadsToDispatch, vector<vector<PListType>*>* patterns);
	vector<vector<string>> ProcessThreadsWorkLoadHD(unsigned int threadsToDispatch, LevelPackage levelInfo, vector<string> prevFileNames);

	void WaitForThreads(vector<unsigned int> localWorkingThreads, vector<future<void>> *localThreadPool, bool recursive = false);

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
