#include "FileProcessor.h"
#include "MemoryUtils.h"
#include "StopWatch.h"
#include "SysMemProc.h"
#include "DiskProc.h"
#include "ChunkFactory.h"
#include <numeric>

FileProcessor::FileProcessor(FileReader* file, 
    ConfigurationParams& _config,
    ProcessorStats& _stats) :
    _config(_config),
    _stats(_stats),
    _processingFinished(false){

    //Kick off thread that processes how much memory the program uses at a certain interval
    auto memoryQueryThread = new thread(&FileProcessor::memoryQuery, this);

    const char * c = file->fileName.c_str();

    // Open the file for the shortest time possible.
    file->copyBuffer = new ifstream(c, ios::binary);

    file->fileString.clear();
    if (_config.processInts)
    {
        FileReader::intToChar(file);
    }
    //If file is not opened then delete copy buffer and discard this file and continue processing with the next file
    if (!file->copyBuffer->is_open())
    {
        //Close file handle once and for all
        file->copyBuffer->clear();
        file->fileString.clear();
        file->fileString.reserve(0);
        delete file;
        return;
    }

    //If the file has not been read in and we are processing only with ram then read in the entire file
    if (file->fileString.size() == 0 && _config.usingPureRAM && !_config.processInts) {

        file->fileString.resize(file->fileStringSize);
        file->copyBuffer->read(&file->fileString[0], file->fileString.size());
    }
    //Process entire for disk processing atm
    else {

        file->fileString.resize(file->fileStringSize);
        file->copyBuffer->read(&file->fileString[0], file->fileString.size());
    }

    auto threadPool = new vector<future<void>>();

    //Preprocess first level and place in buckets
    std::vector<std::vector<PListType>*> leaves;
    for (int i = 0; i < 256; i++) {

        leaves.push_back(new vector<PListType>());
    }
    for (PListType i = 0; i < file->fileStringSize; ++i) {

        uint8_t tempIndex = static_cast<uint8_t>(_config.currentFile->fileString[i]);

        //Only search in memory locations that are divisible by four to prevent incorrect patterns
        //that could exist between integer encodings
        if (_config.processInts && (i % 4 == 0)) {
            leaves[tempIndex]->push_back(i);
        }
        //Otherwise search normally
        else if (!_config.processInts) {
            leaves[tempIndex]->push_back(i);
        }
    }

    //make this value 1 so calculations work correctly then reset
    LevelPackage levelInfo;
    levelInfo.currLevel = 1;
    levelInfo.inceptionLevelLOL = 0;
    levelInfo.threadIndex = 0;
    levelInfo.useRAM = true;
    levelInfo.coreIndex = 0;
    bool prediction = MemoryUtils::DiskOrSysMemProcessing(levelInfo, 1, _config.currentFile->fileStringSize, _config, _stats);

    ThreadCoreMan threadMgr(_config.numThreads);
    Proc::DataBundle bundle = { _config, _stats, threadMgr };

    //use that one prediction
    if (!prediction)
    {
        std::vector<PatternData*> patternsSplit;
        for (PListType i = 0; i < _config.numThreads; i++) {
            patternsSplit.push_back(new PatternData());
        }
        int threadIndex = 0;
        for (auto leaf : leaves) {
            patternsSplit[ threadIndex % _config.numThreads ]->push_back(leaf);
            threadIndex++;
        }

        vector<unsigned int> localWorkingThreads;
        for (unsigned int i = 0; i < patternsSplit.size(); i++)
        {
            localWorkingThreads.push_back(i);
        }

        threadMgr.Lock();
        for (unsigned int i = 0; i < localWorkingThreads.size(); i++)
        {
            threadPool->push_back(std::async([](SysMemProc * proc) {
                proc->Process();
            }, new SysMemProc(patternsSplit[i], bundle, levelInfo)));

        }
        threadMgr.UnLock();
        Proc proc(_config, _stats, threadMgr);
        proc.WaitOnThreads(localWorkingThreads, threadPool, true, 1, threadMgr);
    }
    //HD processing
    else
    {
        //Push patterns to file from whatever is left of processing
        stringstream stringBuilder;
        auto chunkFactorio = ChunkFactory::instance();
        PListType newID = chunkFactorio->GenerateUniqueID();
        stringBuilder << newID;
        std::vector<std::string> files;
        files.push_back(chunkFactorio->CreatePartialPatternFile(stringBuilder.str(), leaves, levelInfo));

        vector<unsigned int> localWorkingThreads;
        for (unsigned int i = 0; i < files.size(); i++)
        {
            localWorkingThreads.push_back(i);
        }
        threadMgr.Lock();
        for (unsigned int i = 0; i < localWorkingThreads.size(); i++)
        {
            //Spawn each hd processing thread with level state information

            threadPool->push_back(std::async([]
            (DiskProc * proc) {
                proc->Process();
            }, new DiskProc(files, bundle, levelInfo)));
        }
        threadMgr.UnLock();
        Proc proc(_config, _stats, threadMgr);
        proc.WaitOnThreads(localWorkingThreads, threadPool, true, 1, threadMgr);
    }

    //Clean up memory watch dog thread
    if (memoryQueryThread != nullptr)
    {
        _processingFinished = true;
        memoryQueryThread->join();
        delete memoryQueryThread;
        memoryQueryThread = nullptr;
        _processingFinished = false;
    }
}

void FileProcessor::memoryQuery()
{
    //Query OS with how much memory the program is using and exit if it has reached the memory limit
    StopWatch swTimer;
    swTimer.Start();
    PListType previousEradicatedPatterns = 0;
    unsigned int overMemoryCountCounter = 0;
    double currMemoryOverflow = 0;
    double mostMemoryOverflow = 0;
    PListType memoryCeiling = (PListType)MemoryUtils::GetAvailableRAMMB() - 1000;
    StopWatch initTime;

    while (!_processingFinished)
    {
        this_thread::sleep_for(std::chrono::milliseconds(1));
        double memoryOverflow = 0;
        _config.overMemoryCount = MemoryUtils::IsOverMemoryCount(_config.memoryUsageAtInception, (double)_config.memoryBandwidthMB, memoryOverflow);

        //Keep track of over memory usage 
        if (_config.overMemoryCount) {
            overMemoryCountCounter++;
        }
        //If memory has been overused for one second be nice and double memory size
        if (overMemoryCountCounter == 1000) {
            _config.memoryBandwidthMB = 2 * _config.memoryBandwidthMB;
            _config.memoryPerThread = _config.memoryBandwidthMB / _config.numThreads;
            overMemoryCountCounter = 0;
            Logger::WriteLog("Memory limit now doubled to: ", _config.memoryBandwidthMB, "\n");
        }

        currMemoryOverflow = memoryOverflow;
        if (mostMemoryOverflow < memoryOverflow)
        {
            mostMemoryOverflow = memoryOverflow;
        }
        //Abort mission and do not exit gracefully ie dump cause we will be pageing soon
        if (currMemoryOverflow + _config.memoryBandwidthMB > memoryCeiling && !_config.usingPureRAM)
        {
            Logger::WriteLog("Have to bail because you are using too much memory for your system!");
            exit(0);
        }

        //Every 10 seconds dispaly the percentage of the file processed, cpu utilization of the program and the approximate time left to finish processing
        if (swTimer.GetTime() > 10000.0f)
        {
            PListType timeStamp = static_cast<PListType>(swTimer.GetTime());
            swTimer.Start();
            Logger::WriteLog("Thread level status...\n");
            for (PListType j = 0; j < _stats.GetCurrentLevelSize(); j++)
            {
                Logger::WriteLog("Thread ", j, " is at level: ", _stats.GetCurrentLevel(j), "\n");
            }

            Logger::WriteLog("Percentage of file processed is: ", (((double)_stats.GetEradicatedPatterns()) / ((double)_config.currentFile->fileStringSize))*100.0f, "%\n");
            Logger::WriteLog("Percentage of cpu usage: ", MemoryUtils::CPULoad(), "%\n");
            Logger::WriteLog("Approximate processing time left: ",
                ((((double)(_config.currentFile->fileStringSize - _stats.GetEradicatedPatterns()))  * timeStamp)
                    / ((double)(_stats.GetEradicatedPatterns() - previousEradicatedPatterns))) / 1000.0f,
                " seconds\n");
            initTime.DisplayNow();

            previousEradicatedPatterns = _stats.GetEradicatedPatterns();
        }
    }
}
