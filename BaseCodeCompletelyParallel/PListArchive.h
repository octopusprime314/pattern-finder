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
const static PListType totalLoops = hdSectorSize/sizeof(PListType);
class PListArchive
{
public:
	PListArchive(void);
	~PListArchive(void);
	PListArchive(string fileName, bool create = false);

	//Load in pList
	void WriteArchiveMapMMAP(const vector<PListType> &pListVector, PatternType pattern = "", bool flush = false);
	void WriteArchiveMapMMAPLargeFile(const vector<PListType> &pListVector, PatternType pattern = "", bool flush = false);
	
	//Write map to hard disk 
	void DumpPatternsToDisk(unsigned int level);

	vector<string>* GetPatterns(unsigned int level, PListType count);
	void GetPListArchiveMMAP(vector<vector<PListType>*> &stuffedPListBuffer, double chunkSizeInMB = 0);
	bool IsEndOfFile();
	bool Exists();
	
	//Close archive
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
	static mutex syncLock;

private:
	static mutex listCountMutex;
	PListType predictedFutureMemoryLocation;
	bool alreadyWrittenLargeFile;
	bool endOfFileReached;
	ofstream *outputFile;
	list<PListType*> memLocals;
	list<char*> charLocals;

	//for mmap writing
	PListType prevListIndex;
	PListType prevStartingIndex;

	void FlushMapList(list<PListType*> memLocalList, list<char*> charLocalList, PListType *mapToDelete);

	void MappingError(int& fileDescriptor, string fileName);
	void UnMappingError(int& fileDescriptor, string fileName);
	void SeekingError(int& fileDescriptor, string fileName);
	void ExtendingFileError(int& fileDescriptor, string fileName);


	int iteration;
	PListType *mapper;
	PListType prevFileIndex;
	bool dumpDeleted;
};


