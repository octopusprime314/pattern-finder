#pragma once
#include "Logger.h"
#include "TypeDefines.h"
#include "FileReader.h"

struct ConfigurationParams
{
	PListType minimum, maximum;
	bool usingPureRAM;
	bool usingPureHD;
	unsigned int numThreads;
	bool findBestThreadNumber;
	int history;
	PatternDiscoveryType nonOverlappingPatternSearch;
	PListType memoryBandwidthMB;
	bool usingMemoryBandwidth;
	unsigned int levelToOutput;
	string patternToSearchFor;
	unsigned int testIterations;
	PListType minOccurrence;
	vector<FileReader*> files;
	int currentFileIndex;
	vector<PListType> fileSizes;
	int threadLimitation;
	unsigned char lowRange;
	unsigned char highRange;
};
class ProcessorConfig
{
public:
	ProcessorConfig(void);
	~ProcessorConfig(void);
	static ConfigurationParams GetConfig(int argc, char **argv);
	
private:
	static ConfigurationParams config;
	static void FindFiles(string directory);
	static void DisplayHelpMessage();
	
};

