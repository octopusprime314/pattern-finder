#pragma once
#include <iostream>
#include <vector>
#include <unordered_map>
#include "TypeDefines.h"
using namespace std;

class TreeRAM
{
private:
	vector<PListType> *pList;

public:
	TreeRAM();
	~TreeRAM();
	TreeRAM(PListType pIndex);
	vector<vector<PListType>*>* GetLeafPLists(PListType& eradicatedPatterns, PListType minOccurence);
	inline void TreeRAM::addLeaf(const unsigned char& uniqueChar, PListType pIndex)
	{
		if (leaves.find(uniqueChar) == leaves.end())
		{
			leaves[uniqueChar] = new TreeRAM(pIndex);
		}
		else
		{
			leaves[uniqueChar]->addPIndex(pIndex);
		}
	}
	inline void TreeRAM::addPIndex(PListType pIndex)
	{
		pList->push_back(pIndex);
	}
	vector<PListType>* GetPList();
	static vector<PListType>* GetMostCommonPattern(vector<vector<PListType>*>* pLists, string buffer, int level);
	unordered_map<char, TreeRAM*> leaves;
};
