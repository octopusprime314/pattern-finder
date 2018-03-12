#include "Proc.h"
#include "ChunkFactory.h"
#include <list>

Proc::Proc(const ConfigurationParams& config,
    ProcessorStats& stats,
    ThreadCoreMan& threadMgr) :
    _config(config),
    _stats(stats),
    _threadMgr(threadMgr) {

}

void Proc::WaitOnThreads(vector<unsigned int> localWorkingThreads,
    vector<future<void>> *localThreadPool,
    bool recursive,
    unsigned int level,
    ThreadCoreMan& threadMgr)
{
    PListType threadsFinished = 0;
    //Poll each processing thread to see if it is done processing and then release thread power
    //to the other threads when a thread is released from proessing
    while (threadsFinished != localThreadPool->size())
    {
        vector<unsigned int> currentThreads;
        for (PListType k = 0; k < localWorkingThreads.size(); k++)
        {

            if (localThreadPool != nullptr && (*localThreadPool)[localWorkingThreads[k]].valid())
            {
                if (recursive)
                {
                    (*localThreadPool)[localWorkingThreads[k]].get();

                    threadsFinished++;
                }
                else
                {
                    (*localThreadPool)[localWorkingThreads[k]].get();
                    threadsFinished++;

                    threadMgr.setThreadStatus(k, false);

                    if (level != 0)
                    {
                        Logger::WriteLog("Thread ", localWorkingThreads[k], " finished all processing", '\n');
                    }
                }
            }
            else
            {
                //std::this_thread::sleep_for(std::chrono::milliseconds(1));
                currentThreads.push_back(localWorkingThreads[k]);
            }
        }
        localWorkingThreads.clear();
        for (unsigned int i = 0; i < currentThreads.size(); i++)
        {
            localWorkingThreads.push_back(currentThreads[i]);
        }
    }
}

void Proc::PrepData(bool prediction, LevelPackage& levelInfo, vector<string>& fileList, vector<vector<PListType>*>* prevLocalPListArray)
{
    ChunkFactory* chunkFactorio = ChunkFactory::instance();

    //If the upcoming level is going to be processed using the hard disk
    if (prediction)
    {
        //If the previous level was processed using the hard disk then just distribute the pattern files across the threads
        if (levelInfo.useRAM)
        {
            //chunk file
            PListArchive* threadFile;
            stringstream threadFilesNames;

            fileList.clear();

            threadFilesNames.str("");
            threadFilesNames << "PListChunks" << chunkFactorio->GenerateUniqueID();

            threadFile = new PListArchive(threadFilesNames.str(), true);
            fileList.push_back(threadFilesNames.str());

            for (PListType i = 0; i < prevLocalPListArray->size(); i++)
            {
                std::list<PListType> *sorting = new std::list<PListType>();
                copy((*prevLocalPListArray)[i]->begin(), (*prevLocalPListArray)[i]->end(), std::back_inserter(*sorting));
                ((*prevLocalPListArray)[i])->erase(((*prevLocalPListArray)[i])->begin(), ((*prevLocalPListArray)[i])->end());
                sorting->sort();
                std::copy(sorting->begin(), sorting->end(), std::back_inserter(*((*prevLocalPListArray)[i])));
                sorting->clear();
                delete sorting;

                threadFile->WriteArchiveMapMMAP(*(*prevLocalPListArray)[i]);

                delete (*prevLocalPListArray)[i];

                if (threadFile->totalWritten >= PListArchive::writeSize)
                {
                    threadFile->WriteArchiveMapMMAP(vector<PListType>(), "", true);
                }

            }
            //Clear out the array also after deletion
            prevLocalPListArray->clear();

            threadFile->WriteArchiveMapMMAP(vector<PListType>(), "", true);
            threadFile->CloseArchiveMMAP();
            delete threadFile;
        }
    }
    //If the upcoming level is going to be processed with ram
    else if (!prediction)
    {
        //If the previous level using hard disk to process then read in pattern file data and convert to pattern vectors
        if (!levelInfo.useRAM)
        {
            for (PListType prevChunkCount = 0; prevChunkCount < fileList.size(); prevChunkCount++)
            {
                PListArchive archive(fileList[prevChunkCount]);
                while (archive.Exists() && !archive.IsEndOfFile())
                {
                    //Just use 100 GB to say we want the whole file for now
                    vector<vector<PListType>*> packedPListArray;
                    archive.GetPListArchiveMMAP(packedPListArray);
                    prevLocalPListArray->insert(prevLocalPListArray->end(), packedPListArray.begin(), packedPListArray.end());

                    packedPListArray.erase(packedPListArray.begin(), packedPListArray.end());
                }
                archive.CloseArchiveMMAP();
            }
            if (!_config.history)
            {
                chunkFactorio->DeletePatternFiles(fileList, ARCHIVE_FOLDER);
            }
            fileList.clear();
        }
    }

    levelInfo.useRAM = !prediction;
    _stats.SetUsingRAM(levelInfo.threadIndex, !prediction);

    //Read in all file data back in if the level is using only ram
    if (levelInfo.useRAM && _config.currentFile->fileString.size() != _config.currentFile->fileStringSize)
    {
        //new way to read in file
        _threadMgr.Lock();
        _config.currentFile->copyBuffer->seekg(0);
        _config.currentFile->fileString.resize(_config.currentFile->fileStringSize);
        _config.currentFile->copyBuffer->read(&_config.currentFile->fileString[0], _config.currentFile->fileString.size());
        _threadMgr.UnLock();
    }
}
