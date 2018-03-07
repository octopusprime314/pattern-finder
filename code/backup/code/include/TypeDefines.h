/** @file TypeDefines.h
 *  @brief Contains all special types used for pattern processing
 *
 *  List of data types for pattern processing
 *
 *  @author Peter J. Morley (pmorley)
 */
#pragma once
#include <string.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#define GREATERTHAN4GB 0
using namespace std;

#define ARCHIVE_FOLDER LOGGERPATH

#if defined(_WIN64) || defined(_WIN32)
#define HELPFILEPATH "../HELP.txt"
#define DATA_FOLDER "../Database/Data/"
#elif defined(__linux__)
#define HELPFILEPATH "../HELP.txt"
#define DATA_FOLDER "../Database/Data/"
#endif

/** Defines the size of pattern index storage. 
 *  If the file size is greater than 4 GB than the address space
 *  size has to be increased from 32 bit to 64 bit storage.
 */
#if GREATERTHAN4GB
typedef unsigned long long PListType;
typedef signed long long PListSignedType;
#else
typedef unsigned long PListType;
typedef signed long PListSignedType;
#endif

typedef string PatternType;

/** Level package keeps track of level processing information */
struct LevelPackage
{
	unsigned int currLevel;
	unsigned int threadIndex;
	unsigned int inceptionLevelLOL;
	unsigned int coreIndex;
	bool useRAM;
};

/** Pattern search methods */
enum PatternDiscoveryType
{
	OVERLAP_PATTERNS,
	NONOVERLAP_PATTERNS,
	ANY_PATTERNS
};