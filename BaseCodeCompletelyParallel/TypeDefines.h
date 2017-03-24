#pragma once
#include <string.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#define GREATERTHAN4GB 0
//#define INTEGERS 1
#define BYTES 1
using namespace std;

#define ARCHIVE_FOLDER LOGGERPATH
#if defined(_WIN64) || defined(_WIN32)
	#define READMEPATH "../../ReadMe.txt"
#elif defined(__linux__)
	#define READMEPATH "../ReadMe.txt"
#endif


#if defined(_WIN64) || defined(_WIN32)
	#define DATA_FOLDER "../../../../Database/Data/"
#elif defined(__linux__)
	#define DATA_FOLDER "../../Database/Data/"
#endif


#if GREATERTHAN4GB
typedef unsigned long long PListType;
#else
typedef unsigned long PListType ;
#endif

#if INTEGERS
typedef unsigned long long PatternType;
#endif

#if BYTES
typedef string PatternType;
#endif

struct LevelPackage
{
	unsigned int currLevel;
	unsigned int threadIndex;
	unsigned int inceptionLevelLOL;
	bool useRAM;
	unsigned int coreIndex;
};