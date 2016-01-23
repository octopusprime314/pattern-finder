#pragma once

#include <iostream>
#include <vector>
#include <map>
#include "TypeDefines.h"
using namespace std;

#define INCREMENTSIZE 256
#define TREENODE_ARRAY 0
#define UNIQUEELEMENTS 256

class TreeHD{
private:
	vector<PListType> *pList;
	

public:

	TreeHD();
	~TreeHD();
	TreeHD(PListType pIndex);
	vector<vector<PListType>*>* GetLeafPLists();
	void addLeaf(PListType pIndex, string pattern);
	void addLeaf(string uniqueChar, PListType pIndex, string pattern);
	void addPIndex(PListType pIndex);
	vector<PListType>* GetPList();
	PListType GetPListCount();
	map<string, TreeHD*> GetMap();
	void ResetMap();
	static void displayTree(vector<TreeHD*> tree, string buffer, int level);
	static vector<PListType>* GetMostCommonPattern(vector<vector<PListType>*>* pLists, string buffer, int level);


	//Make it public so no copying is done
	map<string, TreeHD*> leaves;
};
