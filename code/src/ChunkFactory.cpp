#include "ChunkFactory.h"
#include "Logger.h"

PListType ChunkFactory::fileID = 0;
mutex* ChunkFactory::fileIDMutex = new mutex();

ChunkFactory::ChunkFactory(void)
{
	
}


ChunkFactory::~ChunkFactory(void)
{
}

string ChunkFactory::CreateChunkFile(string fileName, vector<TreeHD>& leaf, LevelPackage levelInfo)
{
	string fileNameToReOpen;

	stringstream archiveName;
	string archiveFileType = "PListChunks";

	PListType count = 0;
	
	for(auto iterator = leaf.begin(); iterator != leaf.end(); iterator++) 
	{
		count += iterator->leaves.size();
	}

	archiveName << archiveFileType << fileName << "_" << count;

	PListArchive* archiveCollective = new PListArchive(archiveName.str(), true);
	fileNameToReOpen = archiveName.str();
	
	for(auto iterator = leaf.begin(); iterator != leaf.end(); iterator++) 
	{
		for(auto iterator2 = iterator->leaves.begin(); iterator2 != iterator->leaves.end(); iterator2++) 
		{
			string pattern = iterator->headLeaf + iterator2->first;
			archiveCollective->WriteArchiveMapMMAP(iterator2->second.pList, pattern, false);
			iterator2->second.pList.clear();
		}
		iterator->leaves.clear();
		//iterator = leaf.erase(iterator);
	}

	vector<TreeHD> test;
	test.swap(leaf);
	leaf.clear();

	archiveCollective->DumpPatternsToDisk(levelInfo.currLevel);
	archiveCollective->WriteArchiveMapMMAP(vector<PListType>(), "", true);
	archiveCollective->CloseArchiveMMAP();

	delete archiveCollective;

	return fileNameToReOpen;
}


string ChunkFactory::CreateChunkFile(string fileName, vector<vector<PListType>*> leaves, LevelPackage levelInfo)
{
	string fileNameToReOpen;

	stringstream archiveName;
	string archiveFileType = "PListChunks";

	archiveName << archiveFileType << fileName << "_256";

	PListArchive* archiveCollective = new PListArchive(archiveName.str(), true);
	fileNameToReOpen = archiveName.str();

	stringstream stringbuilder;

	for(int i = 0; i < 256; i++)
	{
		if(leaves[i]->size() > 0)
		{
			stringbuilder << ((char)i);
			archiveCollective->WriteArchiveMapMMAP(*leaves[i], stringbuilder.str(), false);
			// a full 2MB has to be written to disk before it is worth flushing otherwise there is a major slow down effect from constantly spawning hd flush sync threads
			if(archiveCollective->totalWritten >= PListArchive::writeSize) 
			{
				archiveCollective->WriteArchiveMapMMAP(vector<PListType>(), "", true);
			}

			stringbuilder.str("");
			delete leaves[i];
		}
	}
	
	archiveCollective->DumpPatternsToDisk(levelInfo.currLevel);
	archiveCollective->WriteArchiveMapMMAP(vector<PListType>(), "", true);
	archiveCollective->CloseArchiveMMAP();

	delete archiveCollective;

	return fileNameToReOpen;
}

void ChunkFactory::DeleteChunks(vector<string> fileNames, string folderLocation)
{
	for(int i = 0; i < fileNames.size(); i++)
	{
		DeleteChunk(fileNames[i], folderLocation);
	}
}

void ChunkFactory::DeleteChunk(string fileChunkName, string folderLocation)
{
	string fileNameToBeRemoved = folderLocation;
	fileNameToBeRemoved.append(fileChunkName.c_str());
	fileNameToBeRemoved.append(".txt");
	
	if( remove( fileNameToBeRemoved.c_str() ) != 0)
	{
		/*stringstream builder;
		builder << "Chunk Failed to delete " << fileNameToBeRemoved << ": " << strerror(errno) << '\n';
		Logger::WriteLog(builder.str());*/
	}
	else
	{
		//stringstream builder;
		//builder << "Chunk succesfully deleted " << fileNameToBeRemoved << '\n';
		//Logger::WriteLog(builder.str());
	}

	string fileNameToBeRemovedPatterns = folderLocation;
	fileNameToBeRemovedPatterns.append(fileChunkName.c_str());
	fileNameToBeRemovedPatterns.append("Patterns.txt");
	
	if( remove( fileNameToBeRemovedPatterns.c_str() ) != 0)
	{
		/*stringstream builder;
		builder << "Chunk Failed to delete '" << fileNameToBeRemovedPatterns << "': " << strerror(errno) << '\n';
		Logger::WriteLog(builder.str());*/
	}
	else
	{
		//stringstream builder;
		//builder << "Chunk succesfully deleted " << fileNameToBeRemovedPatterns << '\n';
		//Logger::WriteLog(builder.str());
	}

}

void ChunkFactory::DeleteArchives(vector<string> fileNames, string folderLocation)
{
	for(int i = 0; i < fileNames.size(); i++)
	{
		DeleteArchive(fileNames[i], folderLocation);
	}
}

void ChunkFactory::DeleteArchive(string fileNames, string folderLocation)
{
	string fileNameToBeRemoved = folderLocation;
	fileNameToBeRemoved.append(fileNames.c_str());
	fileNameToBeRemoved.append(".txt");

	if( remove( fileNameToBeRemoved.c_str() ) != 0)
	{
		/*stringstream builder;
		builder << "Archive Failed to delete '" << fileNameToBeRemoved << "': " << strerror(errno) << '\n';
		Logger::WriteLog(builder.str());*/
	}
	else
	{
		//stringstream builder;
		//builder << "Archive succesfully deleted " << fileNameToBeRemoved << '\n';
		//Logger::WriteLog(builder.str());
	}
}