#include "TreeHD.h"
#include "Logger.h"
#include <sstream>


TreeHD::TreeHD()
{
	pList = new vector<PListType>();
}

TreeHD::TreeHD(PListType pIndex)
{
	pList = new vector<PListType>();
	pList->push_back(pIndex);
}

TreeHD::~TreeHD()
{
	leaves.clear();
}

vector<PListType>* TreeHD::GetPList()
{
	return pList;
}

PListType TreeHD::GetPListCount()
{
	return pList->size();
}

map<string, TreeHD*> TreeHD::GetMap()
{
	return leaves;
}

void TreeHD::ResetMap()
{
	leaves.clear();
}

vector<vector<PListType>*>* TreeHD::GetLeafPLists()
{
	vector<vector<PListType>*>* list = new vector<vector<PListType>*>();
	typedef std::map<string, TreeHD*>::iterator it_type;
	for (it_type iterator = leaves.begin(); iterator != leaves.end(); iterator++)
	{
		vector<PListType>* pList = (*iterator).second->GetPList();
		if (pList->size() > 1)
		{
			list->push_back(pList);
		}
		else
		{
			delete (*iterator).second->GetPList();
		}

		delete (*iterator).second;
	}

	return list;
}

void TreeHD::addLeaf(string uniqueChar, PListType pIndex, string pattern)
{
	if (leaves.find(pattern) == leaves.end())
	{
		leaves[pattern] = new TreeHD(pIndex);
	}
	else
	{
		leaves[pattern]->addPIndex(pIndex);
	}
}

void TreeHD::addLeaf(PListType pIndex, string pattern)
{
	if (leaves.find(pattern) == leaves.end())
	{
		leaves[pattern] = new TreeHD(pIndex);
	}
	else
	{
		leaves[pattern]->addPIndex(pIndex);
	}
}

void TreeHD::displayTree(vector<TreeHD*> tree, string buffer, int level)
{
	for (int i = 0; i < tree.size(); i++)
	{
		if (tree[i] != NULL)
		{
			string seq = buffer.substr((*tree[i]->GetPList())[0] - level, level);
			/*stringstream buff;
			buff << seq.c_str() << " is found " << tree[i]->GetPListCount() << endl;
			Logger::WriteLog(buff.str());*/
		}
	}
}


void TreeHD::addPIndex(PListType pIndex)
{
	pList->push_back(pIndex);
}

vector<PListType>* TreeHD::GetMostCommonPattern(vector<vector<PListType>*>* pLists, string buffer, int level)
{
	PListType longestLength = 0;
	PListType longestLengthIndex = 0;
	for (int i = 0; i < pLists->size(); i++)
	{

		if ((*pLists)[i]->size() > longestLength)
		{
			longestLength = (*pLists)[i]->size();
			longestLengthIndex = i;
		}
	}

	if (longestLength > 1)
	{
		//vector<long> *test = (*pLists)[longestLengthIndex];
		//string seq = buffer.substr((*test)[0] - level, level);
		//cout << seq.c_str() << " is found " << longestLength << endl;
		return (*pLists)[longestLengthIndex];
	}
	else
	{
		vector<PListType>* emptyList = new vector<PListType>();
		return NULL;
	}

}



