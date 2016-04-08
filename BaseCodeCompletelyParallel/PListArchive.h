#pragma once
#include <iostream>
#include <sstream>
#include <fstream>
#include <mutex>
#include <vector>
#include <map>
#include <string>
#include "TypeDefines.h"
#if defined(_WIN64) || defined(_WIN32)
	#include "mman.h"
	#include <io.h>
	#include <fcntl.h>
#elif defined(__linux__)
	#include <stdio.h>
	#include <stdlib.h>
	#include <unistd.h>
	#include <sys/io.h>
	#include <sys/mman.h>
	#include <sys/fcntl.h>
	#include <sys/stat.h>
	#include <math.h>
#endif

using namespace std;


class PListArchive
{
public:
	PListArchive(void);
	~PListArchive(void);
	PListArchive(string fileName, bool IsPList = true, bool isBackup = false);

	//Load in pList
	void WriteArchiveMapMMAP(vector<PListType> *pListVector, PatternType pattern = "", bool flush = false);
	void WriteArchiveMapMMAPMeta(vector<PListType> *pListVector, PatternType pattern = "", bool flush = false);
	//Write map to hard disk 
	void DumpMemoryMapMMAPToDisk();
	void DumpPatternsToDisk(unsigned int level);
	void ReadMemoryMapMMAPFromDisk();

	vector<string>* GetPatterns(unsigned int level, PListType count);
	string GetFileChunk(PListType index, PListType chunkSizeInBytes);
	unsigned long long GetFileChunkSize(PListType chunkSizeInBytes);
	vector<vector<PListType>*>* GetPListArchiveMMAP(PListType chunkSizeInMB = 0);
	bool IsEndOfFile();
	bool Exists();
	void DumpContents();

	//Open archive
	void OpenArchiveMMAP();

	//Close archive
	void CloseArchiveMMAP();

	//static PListType patternCount;

	map<PatternType, PListType> GetMetaDataMap();
	vector<PListType>* GetListFromIndex(PListType index);
	
	PListType fileIndex;
	map<PatternType, PListType> pListMetaData;
	string fileName;
	string patternName;
	int fd;
	vector<string> stringBuffer;
	PListType startingIndex;
	PListType mappingIndex;

private:
	
	//map<PListType, string> pListInverseMetaData;
	vector<PListType> pListBuffer;
	ofstream *outputFile;
	PListType fileSize;

	//for mmap writing
	PListType hdSectorSize;
	PListType prevListIndex;
	PListType prevStartingIndex;

	
	PListType prevMappingIndex;

	void MappingError(int& fileDescriptor, string fileName);
	void UnMappingError(int& fileDescriptor, string fileName);
	void SeekingError(int& fileDescriptor, string fileName);
	void ExtendingFileError(int& fileDescriptor, string fileName);

};


