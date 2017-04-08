#pragma once
#include <string>
#include "TreeHD.h"
#include "TypeDefines.h"
#include <vector>
#include "PListArchive.h"
#include <mutex>
using namespace std;


class ChunkFactory
{
public:
	ChunkFactory(void);
	~ChunkFactory(void);

	static ChunkFactory* instance()
	{
		static ChunkFactory chunker;
		return &chunker;
	}

	string CreateChunkFile(string fileName, vector<vector<PListType>*> leaves, LevelPackage levelInfo);

	string CreateChunkFile(string fileName, vector<TreeHD>& leaf, LevelPackage levelInfo);

	void DeleteChunks(vector<string> fileNames, string folderLocation);

	void DeleteChunk(string fileChunkName, string folderLocation);

	void DeleteArchives(vector<string> fileNames, string folderLocation);

	void DeleteArchive(string fileNames, string folderLocation);

	
	PListType GenerateUniqueID()
	{
		PListType id;
		fileIDMutex->lock();
		id = fileID++;
		fileIDMutex->unlock();
		return id;
	}

private:

	static PListType fileID;
	static mutex* fileIDMutex;

};