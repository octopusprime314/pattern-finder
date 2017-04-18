#pragma once
#include "TypeDefines.h"
#include <vector>
using namespace std;

class ProcessorStats
{
public:
	ProcessorStats(void);
	~ProcessorStats(void);

	void SetMostCommonPattern(PListType currLevel, PListType count, PListType index);
	PListType GetMostCommonPatternIndex(PListType currLevel);
	PListType GetMostCommonPatternCount(PListType currLevel);
	vector<PListType> GetMostCommonPatternIndexVector(){ return mostCommonPatternIndex;}
	vector<PListType> GetMostCommonPatternCountVector(){ return mostCommonPatternCount;}

	void SetLevelRecording(PListType currLevel, PListType count);
	PListType GetLevelRecording(PListType currLevel);
	PListType GetLevelRecordingSize(){return static_cast<PListType>(levelRecordings.size());}

	void SetEradicationsPerLevel(PListType currLevel, PListType count);
	PListType GetEradicationsPerLevel(PListType currLevel);

	void SetEradicatedPatterns(PListType count) { eradicatedPatterns = count; }
	PListType GetEradicatedPatterns() { return eradicatedPatterns ;}

	void SetCurrentLevel(PListType thread, PListType level);
	PListType GetCurrentLevel(PListType thread);
	PListType GetCurrentLevelSize(){return static_cast<PListType>(currentLevelVector.size());}

	void SetCoverage(PListType currLevel, float cover);
	float GetCoverage(PListType currLevel);
	vector<float> GetCoverageVector(){ return coverage; }

	void ResetData();

	void SetThreadStatistics(unsigned int threadCount);
	bool GetUsingRAM(unsigned int thread){ return usedRAM[thread]; }
	void SetUsingRAM(unsigned int thread, bool val){ usedRAM[thread] = val; }

private:

	//File statistics
	vector<bool> usedRAM;
	vector<float> coverage;
	vector<bool> activeThreads;
	vector<double> processingTimes;
	vector<PListType> levelRecordings;
	vector<PListType> eradicationsPerLevel;
	vector<PListType> currentLevelVector;
	vector<PListType> mostCommonPatternCount;
	vector<PListType> mostCommonPatternIndex;
	PListType eradicatedPatterns;
};

