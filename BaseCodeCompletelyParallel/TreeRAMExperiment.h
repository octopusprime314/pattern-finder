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
	
	

public:
	TreeRAMExperiment();
	~TreeRAMExperiment();
	TreeRAMExperiment(PListType pIndex);
	void addLeaf(string uniquestring, PListType pIndex);
	void addPIndex(PListType pIndex);
	

	vector<string> *patterns;
	vector<TreeRAMExperiment*> *leaves;
	vector<PListType> *pList;
};
