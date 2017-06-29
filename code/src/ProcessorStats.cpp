/*
 * Copyright (c) 2016, 2017
* The University of Rhode Island
 * All Rights Reserved
 *
 * This code is part of the Pattern Finder.
 *
 * Permission is granted to use, copy, create derivative works and
 * redistribute this software and such derivative works for any
 * purpose, so long as the copyright notice above, this grant of
 * permission, and the disclaimer below appear in all copies made; and
 * so long as the name of The University of Rhode Island is not used in
 * any advertising or publicity pertaining to the use or distribution
 * of this software without specific, written prior authorization.
 *
 * THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION FROM THE
 * UNIVERSITY OF RHODE ISLAND AS TO ITS FITNESS FOR ANY PURPOSE, AND
 * WITHOUT WARRANTY BY THE UNIVERSITY OF RHODE ISLAND OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE. THE UNIVERSITY OF RHODE ISLAND SHALL NOT BE
 * LIABLE FOR ANY DAMAGES, INCLUDING DIRECT, SPECIAL, INDIRECT,
 * INCIDENTAL, OR CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
 * IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGES.
 *
 * Author: Peter J. Morley 
 *          
 */

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
		if(mostCommonPatternIndex[currLevel - 1] == 0 || index < mostCommonPatternIndex[currLevel - 1])
		{
			mostCommonPatternIndex[currLevel - 1] = index;
		}
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

void ProcessorStats::SetTotalOccurrenceFrequency(PListType currLevel, float occurenceFreq)
{
	if(totalOccurrenceFrequency.size() < currLevel)
	{
		totalOccurrenceFrequency.resize(currLevel);
	}
	totalOccurrenceFrequency[currLevel - 1] += occurenceFreq;
}

PListType ProcessorStats::GetTotalOccurrenceFrequency(PListType currLevel)
{
	if(totalOccurrenceFrequency.size() < currLevel)
	{
		totalOccurrenceFrequency.resize(currLevel);
	}
	return totalOccurrenceFrequency[currLevel - 1];
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