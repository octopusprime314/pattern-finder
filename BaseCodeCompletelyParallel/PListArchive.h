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
#include <unordered_map>
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
	PListArchive(string fileName, bool create = false);
	void WriteArchiveMapMMAP(const vector<PListType> &pListVector, const PatternType &pattern = "", bool flush = false, bool forceClose = false);
	void DumpPatternsToDisk(unsigned int level);
	vector<string>* GetPatterns(unsigned int level, PListType count);
	void GetPListArchiveMMAP(vector<vector<PListType>*> &stuffedPListBuffer, double chunkSizeInMB = 0);
	bool IsEndOfFile();
	bool Exists();
	void CloseArchiveMMAP();

	PListType fileIndex;
	string fileName;
	string patternName;
	int fd;
	vector<string> stringBuffer;
	PListType startingIndex;
	PListType mappingIndex;
	PListType fileSize;
	PListType prevMappingIndex;
	static vector<thread*> threadKillList;
	vector<thread*> localThreadList;
	static mutex syncLock;
	list<PListType*> memLocals;
	PListType totalWritten;
	bool created;
	bool patternsDumped;
	bool dataWritten;

	static vector<int> prevFileHandleList;
	static vector<int> newFileHandleList;
	static mutex fileLock;
	static unordered_map<string, int> fileNameToHandleMapping;
	static mutex mapLock;
	static vector<PListType*> mappedList;
	static PListType hdSectorSize;
	static PListType totalLoops;
	static PListType writeSize;

	static mutex charLock;
	static vector<char*> charList;

private:

	bool endOfFileReached;
	ofstream *outputFile;
	list<char*> charLocals;
	PListType prevListIndex;
	PListType prevStartingIndex;
	PListType *mapper;
	PListType prevFileIndex;
	
	void FlushMapList(list<PListType*> memLocalList, list<char*> charLocalList);
	void MappingError(int& fileDescriptor, string fileName);
	void UnMappingError(int& fileDescriptor, string fileName);
	void SeekingError(int& fileDescriptor, string fileName);
	void ExtendingFileError(int& fileDescriptor, string fileName);
};


