
#include "TreeRAMExperiment.h"
#include "Logger.h"
#include <sstream>


TreeRAMExperiment::TreeRAMExperiment()
{
	pList = new vector<PListType>();
	patterns = new vector<string>();
	leaves = new vector<TreeRAMExperiment*>();
}

TreeRAMExperiment::TreeRAMExperiment(PListType pIndex)
{
	pList = new vector<PListType>();
	pList->push_back(pIndex);
	patterns = new vector<string>();
	leaves = new vector<TreeRAMExperiment*>();
}

TreeRAMExperiment::~TreeRAMExperiment()
{
	if(pList != NULL)
	{
		delete pList;
	}
	if(patterns != NULL)
	{
		delete patterns;
	}
	if(leaves != NULL)
	{
		for(int i = 0; i < leaves->size(); i++)
		{
			if((*leaves)[i] != NULL)
			{
				delete (*leaves)[i];
			}
		}
		delete leaves;
	}
}
void TreeRAMExperiment::addLeaf(string uniquestring, PListType pIndex)
{
	bool found = false;
	PListType index = 0;
	for(int i = 0; i < patterns->size(); i++)
	{
		if(uniquestring.compare((*patterns)[i]) == 0)
		{
			found = true;
			index = i;
			break;
		}
	}
	if (!found)
	{
		//patterns->push_back(uniquestring);
		leaves->push_back(new TreeRAMExperiment(pIndex));
	}
	else
	{
		(*leaves)[index]->addPIndex(pIndex);
	}
}

void TreeRAMExperiment::addPIndex(PListType pIndex)
{
	pList->push_back(pIndex);
}

