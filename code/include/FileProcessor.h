#pragma once
#include "FileReader.h"
#include "ProcessorConfig.h"
#include "ProcessorStats.h"
class FileProcessor {

public:
    FileProcessor(FileReader* file, ConfigurationParams& config, ProcessorStats& stats);
    void memoryQuery();

private:
    bool                 _processingFinished;
    ConfigurationParams& _config;
    ProcessorStats&      _stats;
    vector<size_t>       _sortIndexes(const vector<ProcessorStats::DisplayStruct> &v);
};