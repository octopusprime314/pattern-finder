#pragma once
#include <iostream>
#include <vector>
#include <map>
#include "TypeDefines.h"
using namespace std;

class TreeHD
{
private:
	
public:
	TreeHD(){};
	~TreeHD(){};


	inline void setHeadLeaf(string headLeaf)
	{
		this->headLeaf = headLeaf;
	}

	inline void addLeaf(PListType pIndex, char uniquestring)
	{
		if (leaves.find(uniquestring) == leaves.end())
		{
			leaves[uniquestring] = vector<PListType>();
			leaves[uniquestring].push_back(pIndex);
		}
		else
		{
			leaves[uniquestring].push_back(pIndex);
		}		
	}
	map<char, vector<PListType>> leaves;
	string headLeaf;
};
