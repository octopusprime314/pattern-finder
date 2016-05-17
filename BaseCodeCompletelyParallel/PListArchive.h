#pragma once
#include <iostream>
#include <sstream>
#include <fstream>
#include <mutex>
#include <vector>
#include <map>
#include <string>
#include "TypeDefines.h"
#include <list>
#include <thread>
#include <deque>
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
const static PListType hdSectorSize = 2097152;

class PListArchive
{
public:
	PListArchive(void);
	~PListArchive(void);
	PListArchive(string fileName, bool IsPList = true, bool create = false);

	//Load in pList
	//void WriteArchiveMapMMAP(const vector<PListType> *pListVector, PatternType pattern = "", bool flush = false);
	//void WriteArchiveMapMMAP(vector<PListType> pListVector, PatternType pattern = "", bool flush = false);
	void WriteArchiveMapMMAP(const vector<PListType> &pListVector, PatternType pattern = "", bool flush = false);
	//Write map to hard disk 
	void DumpMemoryMapMMAPToDisk();
	void DumpPatternsToDisk(unsigned int level);
	void ReadMemoryMapMMAPFromDisk();


	vector<string>* GetPatterns(unsigned int level, PListType count);
	string GetFileChunk(PListType index, PListType chunkSizeInBytes);
	unsigned long long GetFileChunkSize(PListType chunkSizeInBytes);
	vector<vector<PListType>*>* GetPListArchiveMMAP(PListType chunkSizeInMB = 0);
	void GetPListArchiveMMAP(vector<vector<PListType>*> &stuffedPListBuffer, PListType chunkSizeInMB = 0);
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
	PListType fileSize;

	vector<PListType> pListBuffer;
	PListType prevMappingIndex;
	
	static vector<thread*> threadKillList;
	static mutex syncLock;

private:

	bool endOfFileReached;
	ofstream *outputFile;
	PListType *begMapIndex;
	list<PListType*> memLocals;
	
	//for mmap writing
	PListType prevListIndex;
	PListType prevStartingIndex;

	void FlushMapAsync(PListType *begMapIndex, PListType len);
	void FlushMapList(list<PListType*> memLocalList);

	void MappingError(int& fileDescriptor, string fileName);
	void UnMappingError(int& fileDescriptor, string fileName);
	void SeekingError(int& fileDescriptor, string fileName);
	void ExtendingFileError(int& fileDescriptor, string fileName);

};


