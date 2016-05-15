
#include "TreeRAMExperiment.h"
#include "Logger.h"
#include <sstream>


TreeRAMExperiment::TreeRAMExperiment()
{
	
}

TreeRAMExperiment::TreeRAMExperiment(PListType pIndex)
{
	pList.push_back(pIndex);
}

TreeRAMExperiment::~TreeRAMExperiment()
{
	
}
void TreeRAMExperiment::addLeaf(PListType pIndex, string uniquestring)
{
	if (leaves.find(uniquestring) == leaves.end())
	{
		leaves[uniquestring] = TreeRAMExperiment(pIndex);
	}
	else
	{
		leaves[uniquestring].addPIndex(pIndex);
	}
}

void TreeRAMExperiment::addPIndex(PListType pIndex)
{
	pList.push_back(pIndex);
}

