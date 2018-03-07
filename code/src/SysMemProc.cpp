#include "SysMemProc.h"
#include "DiskProc.h"
#include "MemoryUtils.h"

SysMemProc::SysMemProc(PatternData* patterns,
    DataBundle& bundle,
    LevelPackage _levelInfo) : 
    Proc(bundle.config, bundle.stats, bundle.threadMgr),
    _patterns(patterns),
    _levelInfo(_levelInfo) {

}

void SysMemProc::Process()
{
    
    //Keeps track of all pLists in one contiguous block of memory
    vector<PListType> linearList;
    //Keeps track of the length of each pList
    vector<PListType> pListLengths;

    //Keeps track of all pLists in one contiguous block of memory
    vector<PListType> prevLinearList;
    //Keeps track of the length of each pList
    vector<PListType> prevPListLengths;

    PListType continueSearching = 1;
    PListType totalTallyRemovedPatterns = 0;
    string globalString;
    PListType fileSize = _config.currentFile->fileStringSize;
    PListType totalCount = 0;

    //Process second level slightly differently than the other levels
    if (_levelInfo.currLevel == 2)
    {
        //Take a tally of all the patterns from the previous level
        int threadCountage = 0;
        for (PListType i = 0; i < _patterns->size(); i++)
        {
            PListType pListLength = static_cast<PListType>((*_patterns)[i]->size());
            if (pListLength > 0)
            {
                totalCount += pListLength;
            }
        }
        //Preallocate
        prevLinearList.reserve(totalCount);

        for (PListType i = 0; i < _patterns->size(); i++)
        {
            PListType pListLength = static_cast<PListType>((*_patterns)[i]->size());
            //Greater than zero because we look at every thread's contribution to a pattern
            if (pListLength > 0)
            {
                //Flatten out vector of pattern vectors into a flat vector of patterns 
                threadCountage += pListLength;
                prevLinearList.insert(prevLinearList.end(), 
                    (*_patterns)[i]->begin(), 
                    (*_patterns)[i]->end());
                delete (*_patterns)[i];
            }
            else
            {
                delete (*_patterns)[i];
            }

            //If Hard Disk was processed for the first level then modulo length of pattern
            //based on the number of threads
            if (!_levelInfo.prevLevelProcType)
            {
                if (i % _config.numThreads == (_config.numThreads - 1) && threadCountage > 1)
                {
                    prevPListLengths.push_back(threadCountage);
                    threadCountage = 0;
                }
            }
            else
            {
                prevPListLengths.push_back(threadCountage);
                threadCountage = 0;
            }
        }
    }
    //Process if levels are 3 or greater which doesn't involve combining first level data
    else
    {
        //Get pattern total count
        for (PListType i = 0; i < _patterns->size(); i++)
        {
            PListType pListLength = static_cast<PListType>((*_patterns)[i]->size());
            if (pListLength > 0)
            {
                totalCount += pListLength;
            }
        }

        //Preallocate
        prevLinearList.reserve(totalCount);

        //Convert vector of pattern vectors into a flat vector with an associated vector of pattern lengths
        for (PListType i = 0; i < _patterns->size(); i++)
        {
            PListType pListLength = static_cast<PListType>((*_patterns)[i]->size());
            //This is the only pattern information to look at so it must be greater than one instance
            //to be a pattern
            if (pListLength > 1)
            {
                prevLinearList.insert(prevLinearList.end(), 
                    (*_patterns)[i]->begin(), 
                    (*_patterns)[i]->end());
                prevPListLengths.push_back(pListLength);
                delete (*_patterns)[i];
            }
            else
            {
                delete (*_patterns)[i];
            }
        }
    }
    //Now allocate memory to eventually load pattern information into a contiguous memory vector 
    //instead of hopping around the file by indexing into it
    //based on the patterns
    globalString.resize(totalCount);
    globalString.shrink_to_fit();
    linearList.reserve(totalCount);

    PListType linearListIndex = 0;

    //We have nothing to process!
    if (totalCount == 0)
        return;

    while (continueSearching > 0)
    {

        totalTallyRemovedPatterns = 0;
        PListType stringIndexer = 0;

        //Prep all pattern information for next level processing
        PListType linearListSize = static_cast<PListType>(prevLinearList.size());
        if (linearListSize > 0 && _levelInfo.currLevel == 2 ||
            linearListSize > 1 && _levelInfo.currLevel != 2)
        {
            for (PListType i = 0; i < linearListSize; i++)
            {
                if (prevLinearList[i] < fileSize)
                {
                    globalString[stringIndexer++] =
                        _config.currentFile->fileString[prevLinearList[i]];
                }
            }
        }

        if (prevPListLengths.size() == 0)
        {
            continueSearching = 0;
            break;
        }

        //Shrink vector to be correct size
        globalString.resize(stringIndexer);
        globalString.shrink_to_fit();

        //Build the tree patterns for each previous node
        totalTallyRemovedPatterns = _buildTree(globalString, prevLinearList,
                                    prevPListLengths, linearList,
                                    pListLengths);

  
        //Record all data for post processing reasons
        _buildStats(totalTallyRemovedPatterns,
            linearList,
            pListLengths);


        _levelInfo.currLevel++;

        //Swap previous and next level data for the proceeding level to process
        if (linearList.size() == 0 || _levelInfo.currLevel - 1 >= _config.maximum)
        {
            prevLinearList.clear();
            prevLinearList.reserve(linearList.size());
            prevLinearList.swap((linearList));

            prevPListLengths.clear();
            prevPListLengths.reserve(pListLengths.size());
            prevPListLengths.swap((pListLengths));
            pListLengths.reserve(0);

            continueSearching = 0;
        }
        else
        {
            //Have to add prediction here to see if RAM or HD could be used 
            bool prediction = MemoryUtils::DiskOrSysMemProcessing(_levelInfo, 
                static_cast<PListType>(pListLengths.size()), 
                static_cast<PListType>(linearList.size()), 
                _config, 
                _stats);

            //If we need to switch gears and process using the hard disk for this proceeding level
            if (prediction)
            {

                PListType indexing = 0;
                _patterns->clear();
                for (int i = 0; i < pListLengths.size(); i++)
                {
                    _patterns->push_back(new vector<PListType>(linearList.begin() + indexing, 
                        linearList.begin() + indexing + pListLengths[i]));
                    indexing += pListLengths[i];
                }

                continueSearching = static_cast<PListType>(pListLengths.size());

                linearList.clear();
                linearList.reserve(0);
                pListLengths.clear();
                pListLengths.reserve(0);

                //Instead of breaking, build a disk processing object and start searching
                std::vector<std::string> fileList;_levelInfo.useRAM = false;
                _levelInfo.useRAM = true;
                //Build up the files for processing using hd
                PrepData(prediction, _levelInfo, fileList, _patterns);

                //Need to use this data type because std::async implements fake variadic templates
                //and can only support 6 arguments...fake!
                DataBundle bundle = { _config, _stats, _threadMgr };

                auto diskProc = new DiskProc(fileList, bundle, _levelInfo);
                diskProc->Process();
                continueSearching = 0;
            }
            //If still using RAM we can attempt to dispatch more processing threads for this proceeding level
            else
            {
                continueSearching = static_cast<PListType>(pListLengths.size());

                SplitUpWork(continueSearching, 
                    linearList, 
                    pListLengths);

                prevLinearList.clear();
                prevLinearList.reserve(linearList.size());
                prevLinearList.swap((linearList));
                linearList.resize(prevLinearList.size());

                prevLinearList.shrink_to_fit();
                linearList.shrink_to_fit();
                linearList.clear();

                prevPListLengths.clear();
                prevPListLengths.reserve(pListLengths.size());
                prevPListLengths.swap((pListLengths));
                pListLengths.resize(prevPListLengths.size());

                prevPListLengths.shrink_to_fit();
                pListLengths.shrink_to_fit();
                pListLengths.clear();
            }
        }
    }
    //Return the number of patterns found at this level
    //return continueSearching;
}

bool SysMemProc::SplitUpWork(PListType& morePatternsToFind, 
    vector<PListType> &linearList, 
    vector<PListType> &pListLengths)
{
    bool dispatchedNewThreads = false;
    bool alreadyUnlocked = false;
    _threadMgr.Lock();

    int unusedCores = (_config.numThreads - _threadMgr.getUnusedCores()) + 1;

    if (pListLengths.size() < unusedCores && unusedCores > 1)
    {
        unusedCores = (int)pListLengths.size();
    }
    //Be conservative with thread allocation
    //Only create new thread for work if the new job will have atleast 10 patterns
    //Stack overflow can occur if there are too many little jobs being assigned
    //Need to have an available core, need to still have patterns to search and 
    //need to have more than 1 pattern to be worth splitting up the work
    if (unusedCores > 1 && morePatternsToFind > 0 && pListLengths.size() / unusedCores > 10)
    {

        bool spawnThreads = true;
        //If this thread is at the lowest level of progress spawn new threads
        if (spawnThreads)
        {
            PatternData* prevLocalPListArray = new PatternData();
            PListType indexing = 0;
            for (auto index = 0; index < pListLengths.size(); index++)
            {
                prevLocalPListArray->push_back(new vector<PListType>(linearList.begin() + indexing, 
                    linearList.begin() + indexing + pListLengths[index]));
                indexing += pListLengths[index];
            }

            linearList.clear();
            linearList.reserve(0);
            pListLengths.clear();
            pListLengths.reserve(0);

            vector<vector<PListType>> balancedTruncList = {};
            BalanceWork(balancedTruncList, unusedCores, prevLocalPListArray);

            vector<unsigned int> localWorkingThreads;
            for (unsigned int i = 0; i < balancedTruncList.size(); i++)
            {
                localWorkingThreads.push_back(i);
            }

            if (localWorkingThreads.size() > 1)
            {
                int threadsToTest = _threadMgr.getUnusedCores() - 1;
                if (threadsToTest + localWorkingThreads.size() <= _config.numThreads)
                {

                    for (int z = 0; z < balancedTruncList.size(); z++)
                    {
                        unsigned int tally = 0;
                        for (int d = 0; d < balancedTruncList[z].size(); d++)
                        {
                            tally += (unsigned int)(*prevLocalPListArray)[balancedTruncList[z][d]]->size();
                        }
                    }

                    dispatchedNewThreads = true;

                    LevelPackage _levelInfoRecursion;
                    _levelInfoRecursion.currLevel = _levelInfo.currLevel;
                    _levelInfoRecursion.threadIndex = _levelInfo.threadIndex;
                    _levelInfoRecursion.inceptionLevelLOL = _levelInfo.inceptionLevelLOL + 1;
                    _levelInfoRecursion.useRAM = true;

                    _threadMgr.defunctThread();

                    vector<future<void>> *localThreadPool = new vector<future<void>>();
                    for (PListType i = 0; i < localWorkingThreads.size(); i++)
                    {
                        _threadMgr.dispatchThread();

                        PatternData* splitPatterns = new PatternData();
                        for (auto patternsIndex : balancedTruncList[i])
                        {
                            if ((*prevLocalPListArray)[patternsIndex] != nullptr)
                            {
                                splitPatterns->push_back((*prevLocalPListArray)[patternsIndex]);
                            }
                        }

                        //Need to use this data type because std::async implements fake variadic templates
                        //and can only support 6 arguments...fake!
                        DataBundle bundle = { _config, _stats,_threadMgr };
                        //Lambda thread call
                        localThreadPool->push_back(std::async([](SysMemProc * proc) {
                            proc->Process();
                        }, new SysMemProc(splitPatterns, bundle, _levelInfoRecursion)));
                    }
                    _threadMgr.UnLock();

                    alreadyUnlocked = true;
                    WaitOnThreads(localWorkingThreads, 
                        localThreadPool, 
                        true, 
                        _levelInfo.threadIndex, 
                        _threadMgr);

                    localThreadPool->erase(localThreadPool->begin(), localThreadPool->end());
                    (*localThreadPool).clear();
                    delete localThreadPool;
                    morePatternsToFind = 0;
                    delete prevLocalPListArray;
                }
                else
                {
                    for (int index = 0; index < prevLocalPListArray->size(); index++)
                    {
                        delete (*prevLocalPListArray)[index];
                    }
                    delete prevLocalPListArray;
                }
            }
            else
            {
                for (int index = 0; index < prevLocalPListArray->size(); index++)
                {
                    delete (*prevLocalPListArray)[index];
                }
                delete prevLocalPListArray;
            }
        }
        else
        {

        }
    }
    if (!alreadyUnlocked)
    {
        _threadMgr.UnLock();
    }
    return dispatchedNewThreads;
}

void SysMemProc::BalanceWork(vector<vector<PListType>>& balancedTruncList, 
    unsigned int threadsToDispatch, 
    PatternData* patterns)
{
    //Evenly distribute pattern data by breaking down the pattern vector and adding
    //them to the thread processing pattern vectors
    vector<vector<PListType>> balancedList(threadsToDispatch);
    vector<PListType> balancedSizeList;
    for (PListType i = 0; i < threadsToDispatch; i++)
    {
        balancedSizeList.push_back(0);
    }
    for (PListType i = 0; i < patterns->size(); i++)
    {

        if ((*patterns)[i] != nullptr)
        {
            bool found = false;
            PListType smallestIndex = 0;
            PListType smallestAmount = -1;
            for (PListType z = 0; z < threadsToDispatch; z++)
            {
                if (balancedSizeList[z] < smallestAmount && (*patterns)[i]->size() > 0)
                {
                    smallestAmount = balancedSizeList[z];
                    smallestIndex = z;
                    found = true;
                }
            }
            if (found && (*patterns)[i]->size() > 0)
            {
                balancedSizeList[smallestIndex] += static_cast<PListType>((*patterns)[i]->size());
                balancedList[smallestIndex].push_back(i);
            }
        }
    }
    PListType sizeToTruncate = 0;
    for (unsigned int i = 0; i < threadsToDispatch; i++)
    {
        if (balancedList[i].size() > 0)
        {
            sizeToTruncate++;
        }
    }

    balancedTruncList.resize(sizeToTruncate);

    PListType internalCount = 0;
    for (unsigned int i = 0; i < threadsToDispatch; i++)
    {
        if (balancedList[i].size() > 0)
        {
            balancedTruncList[internalCount] = balancedList[i];
            internalCount++;
        }
    }

    vector<unsigned int> localWorkingThreads;
    for (unsigned int i = 0; i < sizeToTruncate; i++)
    {
        localWorkingThreads.push_back(i);
    }
}

PListType SysMemProc::_buildTree(const std::string& globalString,
    const vector<PListType>& prevLinearList,
    const vector<PListType>& prevPListLengths,
    vector<PListType>& linearList,
    vector<PListType>& pListLengths) {

    PListType prevPListSize = static_cast<PListType>(prevLinearList.size());
    PListType indexes[256] = { 0 };
    PListType indexesToPush[256] = { 0 };
    PListType firstPatternIndex[256] = { 0 };
    int listLength = 0;

    PListType stringIndexer = 0; 
    vector<PListType> newPList[256];
    PListType removedPatterns = 0;

    int currList = 0;
    int currPosition = 0;
    int limit = prevPListLengths[currList];

    for (PListType i = 0; i < prevPListSize; i++)
    {
        //If pattern is past end of string stream then stop counting this pattern
        PListType index = prevLinearList[i];
        if (index < _config.currentFile->fileStringSize)
        {
            uint8_t indexIntoFile = (uint8_t)globalString[stringIndexer++];
            if (firstPatternIndex[indexIntoFile])
            {
                if (newPList[indexIntoFile].empty())
                {
                    newPList[indexIntoFile].push_back(firstPatternIndex[indexIntoFile]);
                }
                newPList[indexIntoFile].push_back(index + 1);
                indexes[indexIntoFile]++;
            }
            else
            {
                firstPatternIndex[indexIntoFile] = index + 1;
                indexes[indexIntoFile]++;
                indexesToPush[listLength++] = indexIntoFile;
            }
        }
        else
        {
            removedPatterns++;
        }

        if (i + 1 == currPosition + limit)
        {
            for (int k = 0; k < listLength; k++)
            {
                PListType insert = static_cast<PListType>(indexes[indexesToPush[k]]);
                if (insert >= _config.minOccurrence)
                {
                    //Process and eliminate patterns if they overlap each other
                    if (_config.nonOverlappingPatternSearch == NONOVERLAP_PATTERNS)
                    {
                        //Monitor number of patterns that do not overlap ie coverage
                        PListType prevIndex = *newPList[indexesToPush[k]].begin();
                        PListType totalTally = 1;

                        linearList.push_back(prevIndex);
                        for (auto it = newPList[indexesToPush[k]].begin() + 1;
                            it != newPList[indexesToPush[k]].end(); it++)
                        {
                            PListType span = *it - prevIndex;
                            if (span >= _levelInfo.currLevel)
                            {
                                totalTally++;
                                linearList.push_back(*it);
                                prevIndex = *it;
                            }
                            else
                            {
                                removedPatterns++;
                            }
                        }
                        if (newPList[indexesToPush[k]].size() == 2 && totalTally == 1)
                        {
                            removedPatterns++;
                            linearList.pop_back();
                        }
                        else
                        {
                            pListLengths.push_back(totalTally);
                        }
                    }
                    //Process all patterns because patterns CAN overlap each other
                    else if (_config.nonOverlappingPatternSearch == OVERLAP_PATTERNS)
                    {
                        //Monitor number of patterns that do not overlap ie coverage
                        PListType prevIndex = *newPList[indexesToPush[k]].begin();
                        PListType totalTally = 1;

                        linearList.push_back(prevIndex);
                        for (auto it = newPList[indexesToPush[k]].begin() + 1;
                            it != newPList[indexesToPush[k]].end(); it++)
                        {
                            PListType span = *it - prevIndex;
                            if (span < _levelInfo.currLevel)
                            {
                                totalTally++;
                                linearList.push_back(*it);
                                prevIndex = *it;
                            }
                            else
                            {
                                prevIndex = *it;
                                removedPatterns++;
                            }
                        }
                        if (newPList[indexesToPush[k]].size() == 2 && totalTally == 1)
                        {
                            removedPatterns++;
                            linearList.pop_back();
                        }
                        else
                        {
                            pListLengths.push_back(totalTally);
                        }
                    }
                    else
                    {
                        pListLengths.push_back(static_cast<PListType>(
                            newPList[indexesToPush[k]].size()));
                        linearList.insert(linearList.end(),
                            newPList[indexesToPush[k]].begin(), newPList[indexesToPush[k]].end());
                    }

                    indexes[indexesToPush[k]] = 0;
                    firstPatternIndex[indexesToPush[k]] = 0;
                    newPList[indexesToPush[k]].clear();
                }
                else if (insert == 1)
                {
                    removedPatterns++;
                    indexes[indexesToPush[k]] = 0;
                    firstPatternIndex[indexesToPush[k]] = 0;
                    newPList[indexesToPush[k]].clear();
                }

            }
            if (currList + 1 < prevPListLengths.size())
            {
                currPosition = i + 1;
                currList++;
                limit = prevPListLengths[currList];
                listLength = 0;
            }
        }
    }
    return removedPatterns;
}

void SysMemProc::_buildStats(const PListType removedPatterns,
    vector<PListType>& linearList,
    vector<PListType>& pListLengths) {

    //Populate level statistics including most common pattern and instance of that pattern
    //and how many patterns were found
    _stats.Lock();

    //Add all of the pattern counts
    for (PListType i : pListLengths)
    {
        _stats.SetTotalOccurrenceFrequency(_levelInfo.currLevel, i);
    }

    //If levelToOutput is not selected but -Pall is set or if -Pall is set and 
    //-Plevel is set to output data only for a specific level
    if (_config.levelToOutput == 0 || _config.levelToOutput == _levelInfo.currLevel)
    {
        //Keeps track of the index in pListLengths vector
        vector<PListType> positionsInLinearList(pListLengths.size());
        PListType pos = 0;
        for (PListType i = 0; i < positionsInLinearList.size(); i++)
        {
            positionsInLinearList[i] = pos;
            pos += pListLengths[i];
        }

        for (PListType z = 0; z < pListLengths.size(); z++)
        {
            PListType distances = 0;
            PListType index = positionsInLinearList[z];
            PListType length = pListLengths[z];
            PListType coverageSubtraction = 0;
            PListType instances = 1;
            //Calculate average distance between pattern instances
            for (auto i = index; i < index + length - 1; i++)
            {
                //Processing ints need to do a different calculation
                if (!_config.processInts ||
                    (_config.processInts &&
                    (linearList[i] - _levelInfo.currLevel) % sizeof(unsigned int) == 0)) {
                    distances += linearList[i + 1] - linearList[i];

                    if (linearList[i + 1] - linearList[i] < _levelInfo.currLevel)
                    {
                        coverageSubtraction += _levelInfo.currLevel - (linearList[i + 1] - linearList[i]);
                    }
                    instances++;
                }
            }
            float averageDistance = ((float)distances) / ((float)(instances - 1));
            stringstream data;

            //Struct used to contain detailed pattern information for one level
            ProcessorStats::DisplayStruct outputData;
            outputData.patternInstances = instances;
            outputData.patternCoveragePercentage = (float)100.0f*(((length*_levelInfo.currLevel) -
                coverageSubtraction)) / (float)_config.currentFile->fileStringSize;
            outputData.averagePatternDistance = averageDistance;
            outputData.firstIndexToPattern = linearList[index] - _levelInfo.currLevel;

            //If pnoname is not selected then strings are written to log, this could be for 
            //reasons where patterns are very long
            if (!_config.suppressStringOutput)
            {
                outputData.pattern = _config.currentFile->fileString.substr(
                    linearList[index] - _levelInfo.currLevel, _levelInfo.currLevel);
            }
            _stats.detailedLevelInfo.push_back(outputData);
        }
    }

    if (_config.processInts) {
        int patterns = 0;
        //Keeps track of the index in pListLengths vector
        vector<PListType> positionsInLinearList(pListLengths.size());
        PListType pos = 0;
        for (PListType i = 0; i < positionsInLinearList.size(); i++)
        {
            positionsInLinearList[i] = pos;
            pos += pListLengths[i];
        }

        for (PListType z = 0; z < pListLengths.size(); z++)
        {
            PListType distances = 0;
            PListType index = positionsInLinearList[z];
            PListType length = pListLengths[z];
            PListType coverageSubtraction = 0;
            PListType instances = 0;
            //Calculate average distance between pattern instances
            for (auto i = index; i < index + length; i++)
            {
                //Processing ints need to do a different calculation
                if (((linearList[i] - _levelInfo.currLevel) % sizeof(unsigned int) == 0)) {
                    instances++;
                }
            }
            if (instances > 1) {
                patterns++;
            }
        }
        _stats.SetLevelRecording(_levelInfo.currLevel,
            _stats.GetLevelRecording(_levelInfo.currLevel) + static_cast<PListType>(patterns));
    }
    else {
        _stats.SetLevelRecording(_levelInfo.currLevel,
            _stats.GetLevelRecording(_levelInfo.currLevel) + static_cast<PListType>(pListLengths.size()));
    }

    _stats.SetEradicationsPerLevel(_levelInfo.currLevel,
        _stats.GetEradicationsPerLevel(_levelInfo.currLevel) + removedPatterns);
    _stats.SetEradicatedPatterns(_stats.GetEradicatedPatterns() + removedPatterns);

    _stats.SetCurrentLevel(_levelInfo.threadIndex, _levelInfo.currLevel + 1);

    PListType tempMostCommonPatternCount = _stats.GetMostCommonPatternCount(_levelInfo.currLevel);

    _stats.UnLock();

    PListType tempMostCommonPatternIndex = 0;
    PListType countage = 0;
    PListType indexOfList = 0;
    bool chosen = false;
    PListType unalteredCount = 0;
    PListType indexToDistance = 0;
    PListType distanceLength = 0;
    for (PListType i = 0; i < pListLengths.size(); i++)
    {
        if (pListLengths[i] > 1)
        {
            PListType prevIndex = linearList[countage];
            PListType tallyCount = 1;

            if (_config.nonOverlappingPatternSearch == OVERLAP_PATTERNS)
            {
                for (PListType j = countage + 1; j < pListLengths[i] + countage; j++)
                {
                    PListType span = linearList[j] - prevIndex;
                    if (span < _levelInfo.currLevel)
                    {
                        prevIndex = linearList[j];
                        tallyCount++;
                    }
                }
            }
            else if (_config.nonOverlappingPatternSearch == NONOVERLAP_PATTERNS)
            {
                for (PListType j = countage + 1; j < pListLengths[i] + countage; j++)
                {
                    PListType span = linearList[j] - prevIndex;
                    if (span >= _levelInfo.currLevel)
                    {
                        prevIndex = linearList[j];
                        tallyCount++;
                    }
                }
            }
            else
            {
                tallyCount = pListLengths[i];
            }
            if (tallyCount > tempMostCommonPatternCount)
            {
                tempMostCommonPatternCount = tallyCount;
                unalteredCount = pListLengths[i];
                indexToDistance = countage;
                distanceLength = pListLengths[i];
                tempMostCommonPatternIndex = linearList[countage] - _levelInfo.currLevel;
                indexOfList = countage;
                chosen = true;
            }
        }
        countage += pListLengths[i];
    }

    //If this level contains the most common pattern that add it
    if (chosen)
    {
        _stats.Lock();
        _stats.SetMostCommonPattern(_levelInfo.currLevel,
            tempMostCommonPatternCount,
            tempMostCommonPatternIndex);
        PListType distances = 0;
        for (PListType j = indexToDistance; j < indexToDistance + distanceLength - 1; j++)
        {
            distances += linearList[j + 1] - linearList[j];
        }
        float averageDistance = ((float)distances) / ((float)(distanceLength - 1));
        _stats.SetDistance(_levelInfo.currLevel, averageDistance);
        _stats.UnLock();

        //Monitor number of patterns that do not overlap ie coverage
        PListType index = indexOfList;
        _stats.Lock();
        PListType count = unalteredCount;
        _stats.UnLock();
        PListType totalTally = 0;
        float percentage = 0;
        PListType prevIndex = 0;
        PListType totalCoverage = 0;
        if (count > 1)
        {
            prevIndex = linearList[index];
            totalTally++;
            totalCoverage += _levelInfo.currLevel;

            for (PListType i = index + 1; i < count + index; i++)
            {
                PListType span = linearList[i] - prevIndex;
                if (span >= _levelInfo.currLevel)
                {
                    PListType pIndex = linearList[i];
                    totalTally++;
                    prevIndex = pIndex;
                    totalCoverage += _levelInfo.currLevel;
                }
            }
            //Coverage of most common pattern per level
            percentage = ((float)(totalCoverage)) / ((float)(_config.currentFile->fileStringSize));
        }

        //Set coverage of pattern at this level
        _stats.Lock();
        _stats.SetCoverage(_levelInfo.currLevel, percentage);
        _stats.UnLock();

    }
}
