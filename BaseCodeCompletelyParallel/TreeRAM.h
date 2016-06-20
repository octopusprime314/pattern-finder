#pragma once
#include <iostream>
#include <vector>
#include <unordered_map>
#include "TypeDefines.h"
#define MAX_TREE_SIZE 256
using namespace std;

class TreeRAM;
typedef std::unordered_map<char, vector<PListType>*>::iterator it_type_tree_ram;

class TreeRAM
{
private:
	

public:
	TreeRAM();
	~TreeRAM();
	vector<vector<PListType>*>* GetLeafPLists(PListType& eradicatedPatterns, PListType minOccurence);
	void GetLeafPLists(PListType& eradicatedPatterns, PListType minOccurence, vector<vector<PListType>*>* list);
	
	//OLD SLOW VERSION
	//inline void TreeRAM::addLeaf(const unsigned char& uniqueChar, PListType pIndex)
	//{
	//	if (leaves.find(uniqueChar) == leaves.end())
	//	{
	//		leaves[uniqueChar] = new TreeRAM(pIndex);
	//	}
	//	else
	//	{
	//		/*if(leaves[uniqueChar]->pList->size() == leaves[uniqueChar]->pList->capacity())
	//		{
	//			leaves[uniqueChar]->pList->reserve(leaves[uniqueChar]->pList->size()*3);
	//		}*/
	//		leaves[uniqueChar]->pList->push_back(pIndex);
	//	}
	//}

	////FASTEST VERSION SO FAR
	//inline void TreeRAM::addLeaf(const char& uniqueChar, PListType pIndex)
	//{
	//	if (leaves.size() != MAX_TREE_SIZE && leaves.find(uniqueChar) == leaves.end())
	//	{
	//		leaves[uniqueChar] = new TreeRAM(pIndex);
	//	}
	//	else
	//	{
	//		leaves[uniqueChar]->pList->push_back(pIndex);
	//	}
	//}

	//TEST VERSION SO FAR
	inline void TreeRAM::addLeaf(const char& uniqueChar, PListType pIndex)
	{
		/*if (leaves.size() != MAX_TREE_SIZE && leaves.find(uniqueChar) == leaves.end())
		{
			leaves[uniqueChar] = new TreeRAM(pIndex);
		}
		else
		{
			leaves[uniqueChar]->pList->push_back(pIndex);
		}*/

		/*if (leaves.size() == MAX_TREE_SIZE)
		{
			leaves[uniqueChar]->pList->push_back(pIndex);
		}
		else if(leaves.find(uniqueChar) == leaves.end())
		{
			leaves[uniqueChar] = new TreeRAM(pIndex);
		}
		else
		{
			leaves[uniqueChar]->pList->push_back(pIndex);
		}*/

		if (leaves.size() == MAX_TREE_SIZE)
		{
			leaves[uniqueChar]->push_back(pIndex);
		}
		else if(leaves.find(uniqueChar) == leaves.end())
		{
			leaves[uniqueChar] = new vector<PListType>(1, pIndex);
		}
		else
		{
			leaves[uniqueChar]->push_back(pIndex);
		}

		/*TreeRAM *tree = new TreeRAM(pIndex);
		pair<it_type_tree_ram,bool> val = leaves.insert(pair<char, TreeRAM*>(uniqueChar, tree));
		
		if(!val.second)
		{
			leaves[uniqueChar]->pList->push_back(pIndex);
		}
		delete tree;*/
	}
	unordered_map<char, vector<PListType>*> leaves;
};
