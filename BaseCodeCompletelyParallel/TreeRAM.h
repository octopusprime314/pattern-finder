#pragma once
#include <iostream>
#include <vector>
#include <unordered_map>
#include "TypeDefines.h"
#define MAX_TREE_SIZE 256
using namespace std;

class TreeRAM
{

private:
	
	unordered_map<char, vector<PListType>*> leaves;

public:
	TreeRAM();
	~TreeRAM();
	vector<vector<PListType>*>* GetLeafPLists(PListType& eradicatedPatterns, PListType minOccurence);
	void GetLeafPLists(PListType& eradicatedPatterns, PListType minOccurence, vector<vector<PListType>*>* list);
	
	inline void addLeaf(const char& uniqueChar, PListType pIndex)
	{
		if (leaves.size() != MAX_TREE_SIZE && leaves.find(uniqueChar) == leaves.end())
		{
			leaves[uniqueChar] = new vector<PListType>(1, pIndex);
		}
		else
		{
			leaves[uniqueChar]->push_back(pIndex);
		}
	}
};
