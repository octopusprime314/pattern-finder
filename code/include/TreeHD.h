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

	TreeHD(PListType pIndex)
	{
		pList.push_back(pIndex);
	}

	inline void setHeadLeaf(string headLeaf)
	{
		this->headLeaf = headLeaf;
	}

	inline void addLeaf(PListType pIndex, char uniquestring)
	{
		if (leaves.find(uniquestring) == leaves.end())
		{
			leaves[uniquestring] = TreeHD(pIndex);
		}
		else
		{
			leaves[uniquestring].addPIndex(pIndex);
		}		
	}

	inline void addPIndex(PListType pIndex)
	{
		pList.push_back(pIndex);
	}
	
	//map<string, TreeHD> leaves;
	map<char, TreeHD> leaves;
	vector<PListType> pList;
	string headLeaf;
};
