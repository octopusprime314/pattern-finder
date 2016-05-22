
#include "TreeHD.h"
#include "Logger.h"
#include <sstream>


TreeHD::TreeHD()
{
	
}

TreeHD::TreeHD(PListType pIndex)
{
	pList.push_back(pIndex);
}

TreeHD::~TreeHD()
{
	
}
void TreeHD::addLeaf(PListType pIndex, string uniquestring)
{
	if (leaves.find(uniquestring) == leaves.end())
	{
		leaves[uniquestring] = TreeHD(pIndex);
	}
	else
	{
		leaves[uniquestring].addPIndex(pIndex);
	}
}

void TreeHD::addPIndex(PListType pIndex)
{
	pList.push_back(pIndex);
}

