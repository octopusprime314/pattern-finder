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

#include "FileReader.h"
#include <sstream>
#include <iostream>
#include <sstream>
#include <fstream>
#include "MemoryUtils.h"

#if defined(_WIN64) || defined(_WIN32)
#include "Dirent.h"
#elif defined(__linux__)
#include "sys/stat.h"
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#endif

FileReader::FileReader(string fileName, bool &isFile, bool openStream)
{
	fileString = "";
	fileStringSize = 0;
	this->fileName = fileName;

	if(openStream)
	{
		const char * c = fileName.c_str();
		
		// Open the file for the shortest time possible.
		copyBuffer = new ifstream(c, ios::binary);

		// Make sure we have something to read.
		if (!copyBuffer->is_open()) 
		{
			//cout << "Could not open file: " << fileName << endl;
			isFile = false;
			copyBuffer->clear();
			copyBuffer->close();
			delete copyBuffer;
			return;
		}
	}
	isFile = true;
	fileStringSize = MemoryUtils::FileSize(fileName);
	if(fileStringSize == 0)
	{
		isFile = false;
	}

}

void FileReader::LoadFile()
{
	//Load the file in memory
	fileString.clear();
	fileString.resize(fileStringSize);
	copyBuffer->read( &fileString[0], fileStringSize);
}

void FileReader::DeleteBuffer()
{
	//Clear out all file memory
	copyBuffer->clear();
	copyBuffer->close();
	delete copyBuffer;
}


FileReader::~FileReader()
{
	//force deallocation of global string
	DeleteBuffer();

	fileString.clear();
	fileString.reserve(0);
	fileString = "";
}

void FileReader::intToChar(FileReader * file)
{
    std::string tempString;
    tempString.resize(file->fileStringSize);
    file->copyBuffer->read(&tempString[0], file->fileStringSize);

    size_t intEndPosition = tempString.find_first_of(","); //Find first instancee of a comma because data is comma seperated
    size_t stringIndex = 0;
    while (intEndPosition != std::string::npos) {

        std::string value = tempString.substr(stringIndex, intEndPosition - stringIndex);
        unsigned int data = std::stoul(value); //parse the numerical string to an unsigned int
                                               //Convert to from string representation of a number to unsigned int and then finally back to a 4 byte string
        file->fileString.push_back((data >> 24) & 0xFF);
        file->fileString.push_back((data >> 16) & 0xFF);
        file->fileString.push_back((data >> 8) & 0xFF);
        file->fileString.push_back((data) & 0xFF);

        stringIndex = intEndPosition + 1;
        intEndPosition = tempString.find_first_of(",", stringIndex); //Find first instancee of a comma because data is comma seperated
    }

    file->fileStringSize = static_cast<PListType>(file->fileString.size());

    file->fileName += "tmp";

    std::ofstream output = ofstream(file->fileName, ios::binary);

    output.write(file->fileString.c_str(), file->fileString.length());

    output.close();

    file->copyBuffer->clear();
    file->copyBuffer->close();
    delete  file->copyBuffer;

    //Reestablish new file
    const char * c = file->fileName.c_str();
    // Open the file for the shortest time possible.
    file->copyBuffer = new ifstream(c, ios::binary);
}


bool FileReader::containsFile(string directory)
{
    bool foundFiles = false;
#if defined(_WIN64) || defined(_WIN32)
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir(directory.c_str())) != nullptr)
    {
        Logger::WriteLog("Files to be processed: \n");
        /* print all the files and directories within directory */
        while ((ent = readdir(dir)) != nullptr)
        {
            if (*ent->d_name)
            {
                string fileName = string(ent->d_name);

                if (!fileName.empty() && fileName.find("PList") != string::npos)
                {
                    foundFiles = true;
                    break;
                }
            }
        }
        closedir(dir);
    }
    else
    {
        //cout << "Problem reading from directory!" << endl;
    }
#elif defined(__linux__)
    DIR *dir;
    struct dirent *entry;

    if (!(dir = opendir(directory.c_str())))
        return false;

    while ((entry = readdir(dir)) != nullptr)
    {
        string fileName = string(entry->d_name);

        if (!fileName.empty() && fileName.find("PList") != string::npos)
        {
            foundFiles = true;
            break;
        }
    }
    closedir(dir);
#endif		
    return foundFiles;
}

