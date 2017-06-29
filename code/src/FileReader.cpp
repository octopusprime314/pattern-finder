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
