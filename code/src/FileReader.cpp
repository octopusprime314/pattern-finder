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
	fileString.clear();
	fileString.resize(fileStringSize);
	copyBuffer->read( &fileString[0], fileStringSize);
}

void FileReader::DeleteBuffer()
{
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
