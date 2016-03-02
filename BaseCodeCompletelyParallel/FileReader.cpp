#include "FileReader.h"
#include <sstream>
#include <iostream>
#include <sstream>
#include <fstream>
#include "MemoryUtils.h"

FileReader::FileReader(string fileName)
{
	fileString = "";
	fileStringSize = 0;
	this->fileName = fileName;

	const char * c = fileName.c_str();
	
	// Open the file for the shortest time possible.
	copyBuffer = new ifstream(c, ios::binary);

	// Make sure we have something to read.
	if (!copyBuffer->is_open()) 
	{
		cout << "Could not open file: " << fileName << endl;

		return;
	}

	fileStringSize = MemoryUtils::FileSize(fileName);

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
	fileString.resize(0);
	fileString = "";
}
