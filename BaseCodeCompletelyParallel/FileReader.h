#pragma once
#include <iostream>
#include <vector>
#include <map>
#include <future>
#include "TypeDefines.h"

using namespace std;

class FileReader
{
private:

public:
	FileReader(string fileName, bool &isFile, bool openStream = false);
	~FileReader();
	void DeleteBuffer();
	void LoadFile();
	string fileString;
	PListType fileStringSize;
	string fileName;
	ifstream *copyBuffer;
};