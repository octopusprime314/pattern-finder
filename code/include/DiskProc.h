#pragma once
#include "TypeDefines.h"
#include "Proc.h"
#include "ProcessorConfig.h"
class SysMemProc;

class DiskProc : public Proc{

public:
    DiskProc(std::vector<std::string>& fileList,
        DataBundle& bundle,
        LevelPackage levelInfo);
    
    /** @brief Processes patterns using disk
    *
    *  Diosk processing is much slower than sysmem processing
    */
    void Process();

    /** @brief Processes the first level using hard disk and ram
    *
    *  The first level is unique because every thread can have patterns
    *  that the other threads may also have so they have must be processed
    *  differently and compiled together at the end of processing the first level
    *
    *  @param positionInFile offset position in the file used by startPatternIndex
    *  @param startPatternIndex index in the file to begin processing
    *  @param numPatternsToSearch number of indexes within the file to search for patterns
    *  @param threadNum the current thread being used to process this chunk of the file
    *  @return void
    */
 
    PListType ProcessChunksAndGenerate(vector<string> fileNamesToReOpen,
        vector<string>& newFileNames,
        LevelPackage& levelInfo);

    bool SplitUpWork(PListType newPatternCount, 
        bool& morePatternsToFind, 
        vector<string> fileList, 
        LevelPackage levelInfo);

    vector<vector<string>> BalanceWork(unsigned int threadsToDispatch,
        LevelPackage levelInfo,
        vector<string> prevFileNames);

private:
    std::vector<std::string>& _fileList;
    LevelPackage _levelInfo;
};