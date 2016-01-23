
#include "TreeRAMExperiment.h"
#include "Logger.h"
#include <sstream>


TreeRAMExperiment::TreeRAMExperiment()
{
	pList = new vector<PListType>();
}

TreeRAMExperiment::TreeRAMExperiment(PListType pIndex)
{
	pList = new vector<PListType>();
	pList->push_back(pIndex);
}

TreeRAMExperiment::~TreeRAMExperiment()
{
}

vector<PListType>* TreeRAMExperiment::GetPList()
{
	return pList;
}

PListType TreeRAMExperiment::GetPListCount()
{
	return pList->size();
}

map<string, TreeRAMExperiment*> TreeRAMExperiment::GetMap()
{
	return leaves;
}

vector<vector<PListType>*>* TreeRAMExperiment::GetLeafPLists(PListType& eradicatedPatterns)
{
	vector<vector<PListType>*>* list = NULL;
	typedef std::map<string, TreeRAMExperiment*>::iterator it_type;
	for (it_type iterator = leaves.begin(); iterator != leaves.end(); iterator++)
	{
		vector<PListType>* pList = (*iterator).second->GetPList();
		if (pList->size() > 1)
		{
			if(list == NULL)
			{
				list = new vector<vector<PListType>*>();
			}
			list->push_back(pList);
		}
		else
		{
			eradicatedPatterns++;
			delete (*iterator).second->GetPList();
		}

		delete (*iterator).second;
	}
	return list;
}

void TreeRAMExperiment::addLeaf(string uniquestring, PListType pIndex)
{
	
	if (leaves.find(uniquestring) == leaves.end())
	{
		leaves[uniquestring] = new TreeRAMExperiment(pIndex);
	}
	else
	{
		leaves[uniquestring]->addPIndex(pIndex);
	}
}
void TreeRAMExperiment::displayTree(vector<TreeRAMExperiment*> tree, string buffer, int level)
{
	for (int i = 0; i < tree.size(); i++)
	{
		if (tree[i] != NULL)
		{
			string seq = buffer.substr((*tree[i]->GetPList())[0] - level, level);
			stringstream buff;
			buff << seq.c_str() << " is found " << tree[i]->GetPListCount() << endl;
			Logger::WriteLog(buff.str());
		}
	}
}


void TreeRAMExperiment::addPIndex(PListType pIndex)
{
	pList->push_back(pIndex);
}

vector<PListType>* TreeRAMExperiment::GetMostCommonPattern(vector<vector<PListType>*>* pLists, string buffer, int level)
{
	PListType PListTypeestLength = 0;
	PListType PListTypeestLengthIndex = 0;
	for (int i = 0; i < pLists->size(); i++)
	{

		if ((*pLists)[i]->size() > PListTypeestLength)
		{
			PListTypeestLength = (*pLists)[i]->size();
			PListTypeestLengthIndex = i;
		}
	}

	if (PListTypeestLength > 1)
	{
		//vector<PListType> *test = (*pLists)[PListTypeestLengthIndex];
		//string seq = buffer.substr((*test)[0] - level, level);
		//cout << seq.c_str() << " is found " << PListTypeestLength << endl;
		return (*pLists)[PListTypeestLengthIndex];
	}
	else
	{
		vector<PListType>* emptyList = new vector<PListType>();
		return NULL;
	}

}
