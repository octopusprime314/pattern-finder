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
	TreeHD();
	~TreeHD();
	TreeHD(PListType pIndex);
	inline void addLeaf(PListType pIndex, const string &uniquestring)
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
	
	map<string, TreeHD> leaves;
	vector<PListType> pList;
};
