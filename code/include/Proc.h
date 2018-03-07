#pragma once
#include "ProcessorConfig.h"
#include "ProcessorStats.h"
#include "ThreadCoreMan.h"
class Proc {

protected:
    const ConfigurationParams& _config;
    ProcessorStats& _stats;
    ThreadCoreMan& _threadMgr;

public:
    Proc(const ConfigurationParams& config,
    ProcessorStats& stats,
    ThreadCoreMan& threadMgr);

    struct DataBundle {
        const ConfigurationParams& config;
        ProcessorStats& stats;
        ThreadCoreMan& threadMgr;
    };


    /** @brief Keeps track of the state of each processing thread
    *
    *  Monitors threads and releases processing holds on a thread when a thread has
    *  finished processing.  This allows other threads that still have work to do
    *  to utilize an extra thread for their processing.  It is a simplified
    *  implementation of a thread pool.
    *
    *  @param localWorkingThreads indexes into the localWorkingThreads that need to be monitored
    *  @param localThreadPool vector containing threads to monitor
    *  @param recursive indicating if these threads were dispatched recursively or not
    *  @param thread current thread being used
    *  @return void
    */
    void WaitOnThreads(vector<unsigned int> localWorkingThreads,
        vector<future<void>> *localThreadPool,
        bool recursive,
        unsigned int level,
        ThreadCoreMan& threadMgr);

    /** @brief Switches pattern data for either ram or hd processing on levels after the first
    *
    *  If the prediction determines that the upc6oming level can be processed
    *  using ram and the previous level was processed with the hd then a data conversion
    *  must happen.  The reverse data conversion must happen if ram was processed
    *  previously and the current level needs to be processed using the hard disk
    *
    *  @param prediction boolean indicating the current level will be processed with ram or hd
    *  @param levelInfo current level processing information
    *  @param fileList hark disk processing data
    *  @param prevLocalPListArray ram processing data
    *  @return void
    */
    void PrepData(bool prediction, 
        LevelPackage& levelInfo, 
        vector<string>& fileList, 
        vector<vector<PListType>*>* prevLocalPListArray);
};
