#pragma once

#include "Proc.h"
class DiskProc;

class SysMemProc : public Proc{


public:
    SysMemProc(PatternData* patterns,
        DataBundle& bundle,
        LevelPackage levelInfo);

    /** @brief Processes patterns using ram
    *
    *  sysmem processing is much faster than disk processing
    */
    void Process();

    /** @brief Evenly distributes pattern data among multiple threads for ram processing
    *
    *  This algorithm looks for available processing threads and if there are enough
    *  patterns to distribute and available threads then new threads are recursively
    *  dispatched and the current thread just listens and waits for the dispatched
    *  threads to finish processing.
    *
    *  @param newPatternCount number of patterns found in the current level search
    *  @param morePatternsToFind PListType indicating the number of patterns found at this level
    *  @param linearList linear vector of all pattern indexes
    *  @param pListLengths the lengths of each pattern index list for accessing linearList
    *  @param levelInfo current level processing information
    *  @param isThreadDefuncted indicates if the thread is deactivated
    *  and there has been recursively spawned threads to improve processing
    *  @return bool indicating the threads have been dispatched or not
    */
    bool SplitUpWork(PListType& morePatternsToFind,
        vector<PListType> &linearList,
        vector<PListType> &pListLengths);

    /** @brief Evenly distributes ram vector data among the threads to be dispatched
    *
    *  Distributes pattern vector information among the threads.
    *
    *  @param threadsToDispatch number of threads that need distributed pattern data
    *  @param patterns vector containing pattern data
    *  @return vector<vector<PListType>> returns distributed ram vector pattern loads
    */
    void BalanceWork(vector<vector<PListType>>& balancedTruncList, 
        unsigned int threadsToDispatch, 
        PatternData* patterns);

private:

    PatternData * _patterns;
    LevelPackage _levelInfo;

    PListType _buildTree(const std::string& globalString,
        const vector<PListType>& prevLinearList,
        const vector<PListType>& prevPListLengths,
        vector<PListType>& linearList,
        vector<PListType>& pListLengths);

    void      _buildStats(const PListType removedPatterns,
        vector<PListType>& linearList,
        vector<PListType>& pListLengths);

};