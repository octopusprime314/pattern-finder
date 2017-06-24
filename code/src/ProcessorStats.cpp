#include "ProcessorStats.h"


ProcessorStats::ProcessorStats(void)
{
	mostCommonPatternIndex.resize(1);
	mostCommonPatternCount.resize(1);
	usedRAM.resize(1);
	coverage.resize(1);
	activeThreads.resize(1);
	processingTimes.resize(1);
	levelRecordings.resize(1);
	eradicationsPerLevel.resize(1);
	currentLevelVector.resize(1);
	averageDistanceVector.resize(1);
	eradicatedPatterns = 0;
}


ProcessorStats::~ProcessorStats(void)
{
}

void ProcessorStats::SetThreadStatistics(unsigned int threadCount)
{
	//Assume start with RAM
	usedRAM.resize(threadCount);
	for(unsigned int i = 0; i < threadCount; i++)
	{
		usedRAM[i] = true;
	}
}

void ProcessorStats::SetMostCommonPattern(PListType currLevel, PListType count, PListType index)
{
	if(mostCommonPatternCount.size() < currLevel)
	{
		mostCommonPatternCount.resize(currLevel);
	}
	if(mostCommonPatternIndex.size() < currLevel)
	{
		mostCommonPatternIndex.resize(currLevel);
	}
	if(count > mostCommonPatternCount[currLevel - 1])
	{
		mostCommonPatternCount[currLevel - 1] = count;
		mostCommonPatternIndex[currLevel - 1] = index;
	}
}

PListType ProcessorStats::GetMostCommonPatternIndex(PListType currLevel)
{ 
	if(mostCommonPatternIndex.size() < currLevel)
	{
		mostCommonPatternIndex.resize(currLevel);
	}
	return mostCommonPatternIndex[currLevel - 1];
}
	
PListType ProcessorStats::GetMostCommonPatternCount(PListType currLevel)
{ 
	if(mostCommonPatternCount.size() < currLevel)
	{
		mostCommonPatternCount.resize(currLevel);
	}
	return mostCommonPatternCount[currLevel - 1];
}

void ProcessorStats::SetLevelRecording(PListType currLevel, PListType count)
{
	if(levelRecordings.size() < currLevel)
	{
		levelRecordings.resize(currLevel);
	}
	if(count > levelRecordings[currLevel - 1])
	{
		levelRecordings[currLevel - 1] = count;
	}
}
	
PListType ProcessorStats::GetLevelRecording(PListType currLevel)
{
	if(levelRecordings.size() < currLevel)
	{
		levelRecordings.resize(currLevel);
	}
	return levelRecordings[currLevel - 1];
}

void ProcessorStats::SetEradicationsPerLevel(PListType currLevel, PListType count)
{
	if(eradicationsPerLevel.size() < currLevel)
	{
		eradicationsPerLevel.resize(currLevel);
	}
	eradicationsPerLevel[currLevel - 1] = count;
}
	
PListType ProcessorStats::GetEradicationsPerLevel(PListType currLevel)
{
	if(eradicationsPerLevel.size() < currLevel)
	{
		eradicationsPerLevel.resize(currLevel);
	}
	return eradicationsPerLevel[currLevel - 1];
}

void ProcessorStats::SetCurrentLevel(PListType thread, PListType level)
{
	if(currentLevelVector.size() < thread + 1)
	{
		currentLevelVector.resize(thread + 1);
	}
	if(level > currentLevelVector[thread])
	{
		currentLevelVector[thread] = level;
	}
}

PListType ProcessorStats::GetCurrentLevel(PListType thread)
{
	if(currentLevelVector.size() < thread + 1)
	{
		currentLevelVector.resize(thread + 1);
	}
	return currentLevelVector[thread];
}

void ProcessorStats::SetCoverage(PListType currLevel, float cover)
{
	if(coverage.size() < currLevel)
	{
		coverage.resize(currLevel);
	}
	if(cover > coverage[currLevel - 1])
	{
		coverage[currLevel - 1] = cover;
	}
}

float ProcessorStats::GetCoverage(PListType currLevel)
{
	if(coverage.size() < currLevel)
	{
		coverage.resize(currLevel);
	}
	return coverage[currLevel - 1];
}

void ProcessorStats::SetDistance(PListType currLevel, float distance)
{
	if(averageDistanceVector.size() < currLevel)
	{
		averageDistanceVector.resize(currLevel);
	}
	if(distance > averageDistanceVector[currLevel - 1])
	{
		averageDistanceVector[currLevel - 1] = distance;
	}
}

float ProcessorStats::GetDistance(PListType currLevel)
{
	if(averageDistanceVector.size() < currLevel)
	{
		averageDistanceVector.resize(currLevel);
	}
	return averageDistanceVector[currLevel - 1];
}

void ProcessorStats::ResetData()
{
	mostCommonPatternIndex.clear();
	mostCommonPatternCount.clear();
	usedRAM.clear();
	coverage.clear();
	activeThreads.clear();
	processingTimes.clear();
	levelRecordings.clear();
	eradicationsPerLevel.clear();
	currentLevelVector.clear();
	averageDistanceVector.clear();
	eradicatedPatterns = 0;
}