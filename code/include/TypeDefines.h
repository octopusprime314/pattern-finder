#pragma once
#include <string.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#define GREATERTHAN4GB 0
#define BYTES 1
using namespace std;

#define ARCHIVE_FOLDER LOGGERPATH
#if defined(_WIN64) || defined(_WIN32)
	#define HELPFILEPATH "../../HELP.txt"
#elif defined(__linux__)
	#define HELPFILEPATH "../HELP.txt"
#endif


#if defined(_WIN64) || defined(_WIN32)
	#define DATA_FOLDER "../../Database/Data/"
#elif defined(__linux__)
	#define DATA_FOLDER "../Database/Data/"
#endif


#if GREATERTHAN4GB
typedef unsigned long long PListType;
typedef signed long long PListSignedType;
#else
typedef unsigned long PListType;
typedef signed long PListSignedType;
#endif

typedef string PatternType;

struct LevelPackage
{
	unsigned int currLevel;
	unsigned int threadIndex;
	unsigned int inceptionLevelLOL;
	unsigned int coreIndex;
	bool useRAM;
};

enum PatternDiscoveryType
{
	OVERLAP_PATTERNS,
	NONOVERLAP_PATTERNS,
	ANY_PATTERNS
};