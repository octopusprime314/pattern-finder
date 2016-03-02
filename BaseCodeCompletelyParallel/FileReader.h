#pragma once
#include <iostream>
#include <vector>
#include <map>
#include <future>
#include "TypeDefines.h"
#define DATA_FOLDER "../../Data/"
using namespace std;

class FileReader
{
private:

public:
	FileReader(string fileName);
	~FileReader();
	void DeleteBuffer();
	void LoadFile();
	string fileString;
	PListType fileStringSize;
	string fileName;
	ifstream *copyBuffer;
};