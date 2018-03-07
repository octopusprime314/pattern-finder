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
    
    void Process();

    PListType ProcessChunksAndGenerate(vector<string> fileNamesToReOpen,
        vector<string>& newFileNames, PListType memDivisor,
        unsigned int threadNum, unsigned int currLevel,
        unsigned int coreIndex);

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