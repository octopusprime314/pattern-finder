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

string ChunkFactory::CreatePartialPatternFile(string fileName, vector<TreeHD>& leaf, LevelPackage levelInfo)
{
	string fileNameToReOpen;

	stringstream archiveName;
	string archiveFileType = "PListChunks";

	PListType count = 0;
	
	//Find out the number of patterns
	for(auto iterator = leaf.begin(); iterator != leaf.end(); iterator++) 
	{
		count += static_cast<PListType>(iterator->leaves.size());
	}

	archiveName << archiveFileType << fileName << "_" << count;

	//Create a new plist archive file to store pattern index information
	PListArchive* archiveCollective = new PListArchive(archiveName.str(), true);
	fileNameToReOpen = archiveName.str();
	
	//Iterate through TreeHD vector and store all pattern vectors into a pattern file
	for(auto iterator = leaf.begin(); iterator != leaf.end(); iterator++) 
	{
		for(auto iterator2 = iterator->leaves.begin(); iterator2 != iterator->leaves.end(); iterator2++) 
		{
			string pattern = iterator->headLeaf + iterator2->first;
			archiveCollective->WriteArchiveMapMMAP(iterator2->second, pattern, false);
			(*iterator2).second.resize(0);
			(*iterator2).second.shrink_to_fit();
		}
		iterator->leaves.clear();
	}

	//Clear out data in vector by shoving it into a temp variable 
	vector<TreeHD> test;
	test.swap(leaf);
	leaf.clear();

	//Write the remaining patterns to file and create the pattern file for pattern association
	archiveCollective->DumpPatternsToDisk(levelInfo.currLevel);
	archiveCollective->WriteArchiveMapMMAP(vector<PListType>(), "", true);
	archiveCollective->CloseArchiveMMAP();

	delete archiveCollective;

	return fileNameToReOpen;
}


string ChunkFactory::CreatePartialPatternFile(string fileName, vector<vector<PListType>*> leaves, LevelPackage levelInfo)
{
	//This function is used for hd first level processing to improve speed
	string fileNameToReOpen;

	stringstream archiveName;
	string archiveFileType = "PListChunks";

	archiveName << archiveFileType << fileName << "_256";

	//Create pattern file
	PListArchive* archiveCollective = new PListArchive(archiveName.str(), true);
	fileNameToReOpen = archiveName.str();

	stringstream stringbuilder;

	//Iterate through pattern vector and store all pattern vectors into a pattern file
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

	//Write the remaining patterns to file and create the pattern file for pattern association
	archiveCollective->DumpPatternsToDisk(levelInfo.currLevel);
	archiveCollective->WriteArchiveMapMMAP(vector<PListType>(), "", true);
	archiveCollective->CloseArchiveMMAP();

	delete archiveCollective;

	return fileNameToReOpen;
}

void ChunkFactory::DeletePartialPatternFiles(vector<string> fileNames, string folderLocation)
{
	//Delete all files in vector
	for(int i = 0; i < fileNames.size(); i++)
	{
		DeletePartialPatternFile(fileNames[i], folderLocation);
	}
}

void ChunkFactory::DeletePartialPatternFile(string fileChunkName, string folderLocation)
{
	//Delete a single parital pattern file
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

void ChunkFactory::DeletePatternFiles(vector<string> fileNames, string folderLocation)
{
	//Delete all complete pattern files
	for(int i = 0; i < fileNames.size(); i++)
	{
		DeletePatternFile(fileNames[i], folderLocation);
	}
}

void ChunkFactory::DeletePatternFile(string fileNames, string folderLocation)
{
	//Delete a single complete pattern file
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