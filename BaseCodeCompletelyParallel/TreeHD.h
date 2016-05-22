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
	void addLeaf(PListType pIndex, string uniquestring);
	void addPIndex(PListType pIndex);
	
	map<string, TreeHD> leaves;
	vector<PListType> pList;
};
