
#include "TreeRAM.h"
#include "Logger.h"
#include <sstream>
#include "Forest.h"

TreeRAM::TreeRAM()
{
	pList = new vector<PListType>();
}

TreeRAM::TreeRAM(PListType pIndex)
{
	pList = new vector<PListType>();
	pList->push_back(pIndex);
}

TreeRAM::~TreeRAM()
{
}

vector<PListType>* TreeRAM::GetPList()
{
	return pList;
}

vector<vector<PListType>*>* TreeRAM::GetLeafPLists(PListType& eradicatedPatterns, PListType minOccurence)
{
	vector<vector<PListType>*>* list = NULL;
	typedef std::unordered_map<char, TreeRAM*>::iterator it_type;
	for (it_type iterator = leaves.begin(); iterator != leaves.end(); iterator++)
	{
		vector<PListType>* pList = (*iterator).second->GetPList();
		if (pList->size() >= minOccurence/* || (Forest::outlierScans && pList->size() == 1)*/)
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

vector<PListType>* TreeRAM::GetMostCommonPattern(vector<vector<PListType>*>* pLists, string buffer, int level)
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

