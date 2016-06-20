
#include "TreeRAM.h"
#include "Logger.h"
#include <sstream>
#include "Forest.h"

TreeRAM::TreeRAM()
{
}

TreeRAM::~TreeRAM()
{
}

vector<vector<PListType>*>* TreeRAM::GetLeafPLists(PListType& eradicatedPatterns, PListType minOccurence)
{
	vector<vector<PListType>*>* list = NULL;
	for (it_type_tree_ram iterator = leaves.begin(); iterator != leaves.end(); iterator++)
	{
		vector<PListType>* pList = (*iterator).second;
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
			delete (*iterator).second;
		}
	}
	return list;
}

void TreeRAM::GetLeafPLists(PListType& eradicatedPatterns, PListType minOccurence, vector<vector<PListType>*>* list)
{
	for (it_type_tree_ram iterator = leaves.begin(); iterator != leaves.end(); iterator++)
	{
		if ((*iterator).second->size() >= minOccurence/* || (Forest::outlierScans && pList->size() == 1)*/)
		{
			list->push_back((*iterator).second);
		}
		else
		{
			eradicatedPatterns++;
			delete (*iterator).second;
		}
	}
}


