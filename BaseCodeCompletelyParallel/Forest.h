#pragma once
#include <iostream>
#include <vector>
#include <map>
#include <future>
#include "TreeHD.h"
#include "TreeRAM.h"
#include "TreeRAMExperiment.h"
#include "FileReader.h"
#include <sstream>
#include <ctime>
#include <iomanip>
#include "PListArchive.h"
#include "StopWatch.h"
using namespace std;
#define ARCHIVE_FOLDER "../Log/"
#define ARCHIVE_BACKUP_FOLDER "../Log/BackupLog/"
#define THREAD_SPAWNING 1

typedef std::vector<map<PatternType, PListType>>::iterator it_type;
typedef std::map<PatternType, PListType>::iterator it_map_type;
typedef std::map<PatternType, vector<PListType>>::iterator it_map_list_type;
typedef std::map<PatternType, vector<PListType>*>::iterator it_map_list_p_type;
struct LevelPackage
{
	unsigned int currLevel;
	unsigned int threadIndex;
	unsigned int inceptionLevelLOL;
};

class Forest
{
private:
	std::map<string, vector<PListType>*> globalMap;
	vector<mutex*> gatedMutexes;

	vector<int> currentLevelVector;

	vector<bool> activeThreads;
	
	PListType threadsDispatched;
	PListType threadsDefuncted;

	vector<future<void>> *threadPool;
	vector<future<TreeHD*>> *threadPlantSeedPoolHD;
	vector<future<TreeRAM*>> *threadPlantSeedPoolRAM;
	FileReader *file;
	std::string::size_type sz;
	unsigned int numThreads;
	unsigned int levelToOutput;
	bool history;
	PListType minimum, maximum;
	PListType memoryBandwidthMB;
	PListType memoryPerCycle;
	PListType memoryPerThread;
	PListType globalLevel;
	//If /d is in commands then display number of patterns found at each level
	bool displayEachLevelSearch;
	//If /c is in commands then cycle from 1 thread to MAX threads on machine and output best thread scheme
	bool findBestThreadNumber;
	unsigned long long levelCountPatterns;
	mutex *countMutex;
	bool usingMemoryBandwidth;
	unsigned int testIterations;

	bool usingPureRAM;
	bool usingPureHD;
	PListType startingLevel;

	vector<vector<string>> prevFileNameList;
	vector<vector<string>> newFileNameList;

	PListType MemoryUsedPriorToThread;
	PListType MemoryUsageAtInception;

	bool usedRAM;
	PListType sizeOfPreviousLevelMB;
	vector<vector<PListType>*>* prevPListArray;
	vector<vector<PListType>*>* globalPListArray;
	PListType eradicatedPatterns;

	vector<PListType> levelRecordings;
	
	StopWatch initTime;


	//Internal memory testing
	TreeRAM* PlantTreeSeedThreadRAM(PListType positionInFile, PListType startPatternIndex, PListType numPatternsToSearch);
	TreeRAM* PlantTreeSeedThreadDynamicRAM(PListType positionInFile, PListType startPatternIndex, PListType numPatternsToSearch, PListType levelStart = 1);
	TreeHD* PlantTreeSeedThreadHD(PListType positionInFile, PListType startPatternIndex, PListType numPatternsToSearch, unsigned int threadNum);
	
	bool NextLevelTreeSearch(PListType level);
	void ThreadedLevelTreeSearch(PListType threadNum);
	
	vector<vector<PListType>*>* ThreadedLevelTreeSearchRAM(PListType startPatternIndex, PListType numPatternsToSearch);
	void ThreadedLevelTreeSearchRecursionListRAM(vector<vector<PListType>*>* patterns, vector<PListType> patternIndexList, PListType numPatternsToSearch, LevelPackage levelInfo);
	void ThreadedLevelTreeSearchRecursionListHD(vector<vector<PListType>*>* patterns, vector<PListType> patternIndexList, PListType numPatternsToSearch, LevelPackage levelInfo);
	
	void ThreadedLevelTreeSearchBatchRAM(vector<vector<PListType>*>* prevLocalPListArray);
	void CommandLineParser(int argc, char **argv);

	bool PredictHardDiskOrRAMProcessing();

	void FirstLevelHardDiskProcessing(vector<string>& backupFilenames, unsigned int z);
	void FirstLevelRAMProcessing();

	string CreateChunkFile(string fileName, TreeHD& leaf, unsigned int threadNum);
	void DeleteChunks(vector<string> fileNames, string folderLocation);

	PListType ProcessChunks(vector<string> fileNamesToReOpen, PListType memDivisor);
	PListType ProcessChunksAndGenerate(vector<string> fileNamesToReOpen, PListType memDivisor, unsigned int threadNum, bool firstLevel = false);

	vector<vector<PListType>> ProcessThreadsWorkLoad(unsigned int threadsToDispatch, vector<vector<PListType>*>* patterns);
	void WaitForThreads(vector<unsigned int> localWorkingThreads, vector<future<void>> *localThreadPool, bool recursive = false);

	void DisplayPatternsFound();

public:
	Forest(int argc, char **argv);
	~Forest();

};
