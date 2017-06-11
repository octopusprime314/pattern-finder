/** @file ProcessorStats.h
 *  @brief ProcessorStats class contains statistic information from processing including pattern data
 *
 *  @author Peter J. Morley (pmorley)
 */
#pragma once
#include "TypeDefines.h"
#include <vector>
using namespace std;

class ProcessorStats
{
public:
	/** @brief Constructor.
     */
	ProcessorStats(void);

	/** @brief Destructor.
     */
	~ProcessorStats(void);

	/** @brief Sets most common pattern for a particular level
	 *
	 *  @param currLevel current level to set most common pattern for
	 *  @param count number of instances the most common pattern occurs
	 *  @param index location into one instance of the pattern in the file
	 *  @return void
	 */
	void SetMostCommonPattern(PListType currLevel, PListType count, PListType index);

	/** @brief Gets most common pattern for a particular level
	 *
	 *  @param currLevel current level to get most common pattern for
	 *  @return PListType returns a level's most common pattern location
	 */
	PListType GetMostCommonPatternIndex(PListType currLevel);

	/** @brief Gets most common pattern's occurrences count for a particular level
	 *
	 *  @param currLevel current level to get most common pattern occurrence count
	 *  @return PListType returns a level's most common pattern occurence count
	 */
	PListType GetMostCommonPatternCount(PListType currLevel);

	/** @brief Gets all level's most common pattern index vector
	 *
	 *  @return vector<PListType> returns all level's most common pattern indexes
	 */
	vector<PListType> GetMostCommonPatternIndexVector(){ return mostCommonPatternIndex;}

	/** @brief Gets all level's most common pattern occurrence vector
	 *
	 *  @return vector<PListType> returns all level's most common pattern occurrences
	 */
	vector<PListType> GetMostCommonPatternCountVector(){ return mostCommonPatternCount;}

	/** @brief Sets occurence count for most common pattern at the designated level
	 *
	 *  @param currLevel current level to set most common pattern occurrence count for
	 *  @param count number of instances the most common pattern occurs
	 *  @return void
	 */
	void SetLevelRecording(PListType currLevel, PListType count);

	/** @brief Gets occurence count for most common pattern at the designated level
	 *
	 *  @param currLevel current level to get most common pattern occurrence count for
	 *  @return PListType number of instances the most common pattern occurs at currLevel
	 */
	PListType GetLevelRecording(PListType currLevel);

	/** @brief Gets size of occurence count vector
	 *
	 *  @return PListType size of the most common pattern occurrence count vector
	 */
	PListType GetLevelRecordingSize(){return static_cast<PListType>(levelRecordings.size());}

	/** @brief Sets indexes that were removed from processing to know how close to processing the whole file
	 *
	 *  @param currLevel current level to set eliminated index count for
	 *  @param count number of instances of indexes that were eliminated
	 *  @return void
	 */
	void SetEradicationsPerLevel(PListType currLevel, PListType count);

	/** @brief Gets elimination count for the designated level
	 *
	 *  @param currLevel current level to get elimination count
	 *  @return PListType number of eliminated indexes
	 */
	PListType GetEradicationsPerLevel(PListType currLevel);

	/** @brief Sets total indexes eliminated from searching all levels
	 *
	 *  @param count number of instances of indexes that were eliminated
	 *  @return void
	 */
	void SetEradicatedPatterns(PListType count) { eradicatedPatterns = count; }

	/** @brief Gets total indexes eliminated from searching all levels
	 *
	 *  @return PListType number of eliminated indexes from searching all levels
	 */
	PListType GetEradicatedPatterns() { return eradicatedPatterns ;}

	/** @brief Sets current level that a thread is processing 
	 *
	 *  @param thread current thread that is processing at this level
	 *  @param level level being processed
	 *  @return void
	 */
	void SetCurrentLevel(PListType thread, PListType level);

	/** @brief Gets current level a thread is processing 
	 *
	 *  @param thread querying thread that is being processed
	 *  @return PListType level this thread is processing
	 */
	PListType GetCurrentLevel(PListType thread);

	/** @brief Gets current level vector size
	 *
	 *  @return PListType size of current level vector
	 */
	PListType GetCurrentLevelSize(){return static_cast<PListType>(currentLevelVector.size());}

	/** @brief Sets coverage percentage that the most common pattern has over the entire file 
	 *
	 *  @param currLevel current level that has this coverage
	 *  @param cover coverage the pattern has throughout the file
	 *  @return void
	 */
	void SetCoverage(PListType currLevel, float cover);

	/** @brief Gets coverage of the most common pattern at a certain level
	 *
	 *  @param currLevel level coverage
	 *  @return float returns most common patterns coverage at a level
	 */
	float GetCoverage(PListType currLevel);

	/** @brief Gets coverage vector
	 *
	 *  @return PListType returns coverage vector
	 */
	vector<float> GetCoverageVector(){ return coverage; }
	
	/** @brief Resets statistic data used for large dataset processing
	 *
	 *  @return void
	 */
	void ResetData();

	/** @brief Initializes some thread statistics
	 *
	 *  @param threadCount number of threads to be used for processing
	 *  @return void
	 */
	void SetThreadStatistics(unsigned int threadCount);

	/** @brief Get level's status of using ram or hd processing
	 *
	 *  @param thread current thread
	 *  @return bool true if using ram and false if using hd
	 */
	bool GetUsingRAM(unsigned int thread){ return usedRAM[thread]; }

	/** @brief Set level's status of using ram or hd processing
	 *
	 *  @param thread current thread
	 *  @param val bool telling if thread is using ram or hd
	 *  @return void
	 */
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

