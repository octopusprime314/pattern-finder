/*
 * Copyright (c) 2016, 2017
* The University of Rhode Island
 * All Rights Reserved
 *
 * This code is part of the Pattern Finder.
 *
 * Permission is granted to use, copy, create derivative works and
 * redistribute this software and such derivative works for any
 * purpose, so long as the copyright notice above, this grant of
 * permission, and the disclaimer below appear in all copies made; and
 * so long as the name of The University of Rhode Island is not used in
 * any advertising or publicity pertaining to the use or distribution
 * of this software without specific, written prior authorization.
 *
 * THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION FROM THE
 * UNIVERSITY OF RHODE ISLAND AS TO ITS FITNESS FOR ANY PURPOSE, AND
 * WITHOUT WARRANTY BY THE UNIVERSITY OF RHODE ISLAND OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE. THE UNIVERSITY OF RHODE ISLAND SHALL NOT BE
 * LIABLE FOR ANY DAMAGES, INCLUDING DIRECT, SPECIAL, INDIRECT,
 * INCIDENTAL, OR CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
 * IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGES.
 *
 * Author: Peter J. Morley 
 *          
 */

#include "ChunkFactory.h"

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
		count += static_cast<PListType>(iterator->_leaves.size());
	}

    if (count == 0) {
        return std::string("");
    }

	archiveName << archiveFileType << fileName << "_" << count;

	//Create a new plist archive file to store pattern index information
	PListArchive* archiveCollective = new PListArchive(archiveName.str(), true);
	fileNameToReOpen = archiveName.str();
	
	//Iterate through TreeHD vector and store all pattern vectors into a pattern file
	for(auto iterator = leaf.begin(); iterator != leaf.end(); iterator++) 
	{
		for(auto iterator2 = iterator->_leaves.begin(); iterator2 != iterator->_leaves.end(); iterator2++) 
		{
			string pattern = iterator->_headLeaf + iterator2->first;
			archiveCollective->WriteArchiveMapMMAP(iterator2->second, pattern, false);
			(*iterator2).second.resize(0);
			(*iterator2).second.shrink_to_fit();
		}
		iterator->_leaves.clear();
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