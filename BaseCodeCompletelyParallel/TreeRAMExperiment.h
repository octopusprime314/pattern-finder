#pragma once
#include <iostream>
#include <vector>
#include <map>
#include "TypeDefines.h"
using namespace std;

#define INCREMENTSIZE 256
#define TREENODE_ARRAY 0
#define UNIQUEELEMENTS 256

#define ALLOC_SIZE 1024/sizeof(PListType)
#define PRE_ALLOC_TC_MALLOC 0

class TreeRAMExperiment
{
private:
	vector<PListType> *pList;
	map<string, TreeRAMExperiment*> leaves;

public:
	TreeRAMExperiment();
	~TreeRAMExperiment();
	TreeRAMExperiment(PListType pIndex);
	vector<vector<PListType>*>* GetLeafPLists(PListType& eradicatedPatterns);
	void addLeaf(string uniquestring, PListType pIndex);
	void addPIndex(PListType pIndex);
	vector<PListType>* GetPList();
	PListType GetPListCount();
	map<string, TreeRAMExperiment*> GetMap();
	static void displayTree(vector<TreeRAMExperiment*> tree, string buffer, int level);
	static vector<PListType>* GetMostCommonPattern(vector<vector<PListType>*>* pLists, string buffer, int level);
};
