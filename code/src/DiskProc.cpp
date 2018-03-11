#include "DiskProc.h"
#include "SysMemProc.h"
#include "MemoryUtils.h"
#include "PListArchive.h"
#include "TreeHD.h"
#include "ChunkFactory.h"
#include <list>

DiskProc::DiskProc(std::vector<std::string>& fileList,
    DataBundle& bundle,
    LevelPackage levelInfo) : 
    Proc(bundle.config, bundle.stats, bundle.threadMgr),
    _fileList(fileList),
    _levelInfo(levelInfo) {


}

void DiskProc::Process()
{
    double threadMemoryConsumptionInMB = MemoryUtils::GetProgramMemoryConsumption();
    int threadNum = _levelInfo.threadIndex;
    auto chunkFactorio = ChunkFactory::instance();
    PListType newPatternCount = 0;
    bool morePatternsToFind = true;
    try
    {
        while (morePatternsToFind)
        {

            //Divide between file load and previous level pLists and leave some memory for new lists 
            PListType memDivisor = (PListType)(((_config.memoryPerThread * 1000000.0f) / 3.0f));

            unsigned int fileIters = (unsigned int)(_config.currentFile->fileStringSize / memDivisor);
            if (_config.currentFile->fileStringSize%memDivisor != 0)
            {
                fileIters++;
            }

            PListType removedPatterns = 0;
            newPatternCount = 0;
            vector<string> fileNamesToReOpen;
            vector<string> newFileNames;

            unsigned int currLevel = _levelInfo.currLevel;
            //Use previous pattern files to process new files
            for (PListType prevChunkCount = 0; prevChunkCount < _fileList.size(); prevChunkCount++)
            {
                PListArchive archive(_fileList[prevChunkCount]);

                while (!archive.IsEndOfFile())
                {
                    //Pull in 1/3 of the memory bandwidth given for this thread
                    vector<vector<PListType>*> packedPListArray;
                    archive.GetPListArchiveMMAP(packedPListArray, static_cast<double>(memDivisor) / 1000000.0f);

                    if (packedPListArray.size() > 0)
                    {
                        PListType packedListSize = static_cast<PListType>(packedPListArray.size());
                        vector <PListType> prevLeafIndex(packedListSize, 0);

                        //Get minimum and maximum indexes so we can see if some chunks can be skipped from being loaded bam!
                        PListType minimum = -1;
                        PListType maximum = 0;
                        for (int m = 0; m < packedPListArray.size(); m++)
                        {
                            for (int n = 0; n < packedPListArray[m]->size(); n++)
                            {
                                if ((*packedPListArray[m])[0] < minimum)
                                {
                                    minimum = (*packedPListArray[m])[0];
                                }
                            }
                        }

                        unsigned int firstIndex = (unsigned int)(minimum / memDivisor);
                        unsigned int lastIndex = fileIters;

                        std::string fileChunks;

                        std::vector<std::string> saveOffPreviousStringData;
                        saveOffPreviousStringData.resize(fileIters);

                        //Pull in 1/3 of the memory bandwidth for the actual file data
                        for (unsigned int j = firstIndex; j < lastIndex && minimum != -1; j++)
                        {
                            if (packedListSize > 0)
                            {
                                if (fileChunks.size() > 0)
                                {
                                    if (saveOffPreviousStringData[j - 1].size() == 0) {
                                        saveOffPreviousStringData[j - 1] = fileChunks.substr(fileChunks.size() - (currLevel - 1), currLevel - 1);
                                    }
                                }

                                PListType patternCount = 0;
                                if (_config.currentFile->fileStringSize <= memDivisor * j + memDivisor)
                                {
                                    patternCount = _config.currentFile->fileStringSize - memDivisor * j;
                                }
                                else
                                {
                                    patternCount = memDivisor;
                                }

                                //Shared file space test
                                fileChunks = "";
                                fileChunks.resize(patternCount);

                                PListType offset = memDivisor * j;
                                bool isFile;
                                FileReader fileReaderTemp(_config.currentFile->fileName, isFile, true);
                                fileReaderTemp.copyBuffer->seekg(offset);
                                fileReaderTemp.copyBuffer->read(&fileChunks[0], patternCount);
                            }

                            bool justPassedMemorySize = false;


                            vector<TreeHD> leaflet;

                            for (PListType i = 0; i < packedListSize; i++)
                            {
                                vector<PListType>* pList = packedPListArray[i];
                                PListType pListLength = static_cast<PListType>(packedPListArray[i]->size());
                                PListType k = prevLeafIndex[i];

                                string headPattern = "";
                                bool grabbedHead = false;

                                leaflet.push_back(TreeHD());

                                PListSignedType relativeIndex = 0;
                                PListType indexForString = 0;
                                while (k < pListLength && ((*pList)[k]) < (j + 1)*memDivisor)
                                {
                                
                                    PListType nextPatternIndex = (((*pList)[k]) - memDivisor * j);
                                    PListType patternLength = currLevel - 1;
                                    PListType originPatternIndex = nextPatternIndex - patternLength;

                                    try
                                    {
                                        if (((*pList)[k]) < _config.currentFile->fileStringSize)
                                        {
                                            //If index comes out to be larger than fileString than it is a negative number 
                                            //and we must use previous string data!
                                            if (originPatternIndex >= _config.currentFile->fileStringSize)
                                            {
                                                relativeIndex = originPatternIndex;
                                                string pattern = "";
                                                relativeIndex *= -1;
                                                indexForString = static_cast<PListType>(saveOffPreviousStringData[j - 1].size()) - relativeIndex;
                                                if (saveOffPreviousStringData[j - 1].size() > 0 && relativeIndex > 0)
                                                {
                                                    pattern = saveOffPreviousStringData[j - 1].substr(indexForString, relativeIndex);
                                                    pattern.append(fileChunks.substr(0, currLevel - pattern.size()));
                                                    leaflet.back().addLeaf((*pList)[k] + 1, pattern.back());

                                                    if (!grabbedHead)
                                                    {
                                                        grabbedHead = true;
                                                        headPattern = pattern.substr(0, currLevel - 1);
                                                        leaflet.back().setHeadLeaf(headPattern);
                                                    }

                                                }
                                            }
                                            else
                                            {
                                                //If pattern is past end of string stream then stop counting this pattern
                                                if (((*pList)[k]) < _config.currentFile->fileStringSize)
                                                {
                                                    leaflet.back().addLeaf((*pList)[k] + 1, fileChunks[nextPatternIndex]);

                                                    if (!grabbedHead)
                                                    {
                                                        grabbedHead = true;
                                                        headPattern = fileChunks.substr(originPatternIndex, currLevel - 1);
                                                        leaflet.back().setHeadLeaf(headPattern);
                                                    }
                                                }
                                                else if (originPatternIndex < 0)
                                                {
                                                    cout << "String access is out of bounds at beginning" << endl;
                                                }
                                                else if (nextPatternIndex >= _config.currentFile->fileStringSize)
                                                {
                                                    cout << "String access is out of bounds at end" << endl;
                                                }
                                            }
                                        }
                                        else
                                        {
                                            removedPatterns++;
                                        }
                                    }
                                    catch (exception e)
                                    {
                                        cout << "Exception at global index: " << (*pList)[k]
                                            << "Exception at relative index: " << originPatternIndex
                                             << " and computed relative index: " << relativeIndex 
                                             << " and index for string: " << indexForString 
                                             << " System exception: " << e.what() << endl;
                                    }
                                    k++;
                               
                                }
                                prevLeafIndex[i] = k;
                                justPassedMemorySize = false;
                            }

                            //Write remaining patterns to file
                            if (leaflet.size() > 0)
                            {
                                stringstream stringBuilder;
                                PListType newID = chunkFactorio->GenerateUniqueID();
                                stringBuilder << newID;
                                std::string fileNameToUse = chunkFactorio->CreatePartialPatternFile(stringBuilder.str(), leaflet, _levelInfo);
                                if (!fileNameToUse.empty()) {
                                    fileNamesToReOpen.push_back(fileNameToUse);
                                }
                            }

                        }

                        //Deallocate the previous level data
                        for (PListType p = 0; p < packedPListArray.size(); p++)
                        {
                            delete packedPListArray[p];
                        }
                    }
                }

                archive.CloseArchiveMMAP();
            }

            //Take all of the partial pattern files that were generated above and pull together all patterns to compile full pattern data
            newPatternCount += ProcessChunksAndGenerate(fileNamesToReOpen, newFileNames, _levelInfo);

            //Delete processing files
            if (!_config.history)
            {
                chunkFactorio->DeletePatternFiles(_fileList, ARCHIVE_FOLDER);
            }

            _fileList.clear();
            for (int i = 0; i < newFileNames.size(); i++)
            {
                _fileList.push_back(newFileNames[i]);
            }
            fileNamesToReOpen.clear();
            newFileNames.clear();

            //Record hd processing statistics
            _stats.Lock();
            _stats.SetEradicationsPerLevel(currLevel, _stats.GetEradicationsPerLevel(currLevel) + removedPatterns);
            _stats.SetEradicatedPatterns(_stats.GetEradicatedPatterns() + removedPatterns);
            _stats.UnLock();

            if (newPatternCount > 0 && _levelInfo.currLevel < _config.maximum)
            {
                _levelInfo.currLevel++;
                //Have to add prediction here to see if processing needs to be done in ram or not
                bool prediction = MemoryUtils::DiskOrSysMemProcessing(_levelInfo, 
                    newPatternCount, 
                    (_config.currentFile->fileStringSize - _stats.GetEradicatedPatterns()) / (_config.numThreads),
                    _config,
                    _stats);

                if (!prediction)
                {
                    //Instead of breaking, build a disk processing object and start searching
                    PatternData* patterns = new PatternData();
                    _levelInfo.useRAM = false;
                    //Build up the files for processing using hd
                    PrepData(prediction, _levelInfo, _fileList, patterns);

                    //Need to use this data type because std::async implements fake variadic templates
                    //and can only support 6 arguments...fake!
                    DataBundle bundle = { _config, _stats, _threadMgr };

                    auto sysMemProc = new SysMemProc(patterns, bundle, _levelInfo);
                    sysMemProc->Process();

                    morePatternsToFind = false;
                    break;
                }
                else
                {

                    morePatternsToFind = true;
                    //If more threads are available then dispatch multiple threads to process the next level pattern data
                    SplitUpWork(newPatternCount, morePatternsToFind, _fileList, _levelInfo);
                }
            }
            else
            {
                //Delete remaining partial files if not patterns are left to process
                chunkFactorio->DeletePatternFiles(_fileList, ARCHIVE_FOLDER);
                morePatternsToFind = false;
            }
        }
    }
    catch (exception e)
    {
        cout << e.what() << endl;
    }
}

PListType DiskProc::ProcessChunksAndGenerate(vector<string> fileNamesToReOpen, vector<string>& newFileNames, LevelPackage& levelInfo)
{
    auto chunkFactorio = ChunkFactory::instance();
    int currentFile = 0;
    int prevCurrentFile = currentFile;
    bool memoryOverflow = false;
    PListType interimCount = 0;
    unsigned int threadNumber = 0;
    PListType levelCount = 0;

    //Grab all the partial pattern files to bring together into one coherent pattern file structure
    vector<string> fileNamesBackup;
    for (int a = 0; a < fileNamesToReOpen.size(); a++)
    {
        fileNamesBackup.push_back(fileNamesToReOpen[a]);
    }

    vector<PListArchive*> patternFiles;
    PListType internalRemovedCount = 0;

    while (currentFile < fileNamesBackup.size())
    {
        memoryOverflow = false;

        vector<PListArchive*> archiveCollection;
        map<PatternType, vector<PListType>*> finalMetaDataMap;

        for (int a = 0; a < fileNamesBackup.size() - prevCurrentFile; a++)
        {

            archiveCollection.push_back(new PListArchive(fileNamesBackup[a + prevCurrentFile]));

            //If file size is 0 or does not exist, we skip
            if (archiveCollection[a]->IsEndOfFile())
            {
                if (!memoryOverflow)
                {
                    currentFile++;
                }
                archiveCollection[a]->CloseArchiveMMAP();
                delete archiveCollection[a];
                continue;
            }

            //Our job is to trust whoever made the previous partial file made it within the acceptable margin of error so we compensate by saying take up to double the size if the previous
            //parital file went a little over the allocation constraint

            vector<vector<PListType>*> packedPListArray;
            vector<string> *stringBuffer = nullptr;
            PListArchive *stringBufferFile = nullptr;
            string fileNameForLater = "";
            PListType packedPListSize = 0;
            string fileName = "";
            bool foundAHit = true;

            //If memory has not been exceeded then continue compiling pattern map data
            if (!memoryOverflow)
            {
                //Loads in the index vectors where patterns live and also pulls in the actual pattern strings from the Patterns files
                archiveCollection[a]->GetPListArchiveMMAP(packedPListArray);
                packedPListSize = static_cast<PListType>(packedPListArray.size());

                std::string::size_type i = archiveCollection[a]->fileName.find(".txt");
                std::string::size_type begin = archiveCollection[a]->fileName.find("P");
                string tempString = archiveCollection[a]->fileName;
                tempString.erase(i);
                tempString.erase(0, begin);

                fileNameForLater.append(tempString);
                fileName = fileNameForLater;
                fileName.append("Patterns");
                stringBufferFile = new PListArchive(fileName);
                std::string::size_type l = archiveCollection[a]->fileName.find("_");
                string copyString = archiveCollection[a]->fileName;
                copyString.erase(0, l + 1);
                std::string::size_type k = copyString.find(".txt");
                copyString.erase(k, 4);
                std::string::size_type sz;   // alias of size_t
                PListType sizeOfPackedPList = static_cast<PListType>(std::stoll(copyString, &sz));
                stringBuffer = stringBufferFile->GetPatterns(levelInfo.currLevel, packedPListSize);

            }
            else
            {
                //If memory has been overused in the final pattern map then we see which patterns can be stored in file and which need to be kept around
                std::list<string> patternsThatCantBeDumped;
                PListType totalPatterns = 0;

                for (int earlyWriteIndex = a; earlyWriteIndex < fileNamesBackup.size() - prevCurrentFile; earlyWriteIndex++)
                {

                    string tempString = fileNamesBackup[earlyWriteIndex + prevCurrentFile];

                    tempString.append("Patterns");
                    PListArchive *stringBufferFileLocal = new PListArchive(tempString);

                    std::string::size_type j = fileNamesBackup[earlyWriteIndex + prevCurrentFile].find("_");
                    string copyString = fileNamesBackup[earlyWriteIndex + prevCurrentFile];
                    copyString.erase(0, j + 1);
                    std::string::size_type sz;   // alias of size_t
                    PListType sizeOfPackedPList = static_cast<PListType>(std::stoll(copyString, &sz));
                    vector<string> *stringBufferLocal = stringBufferFileLocal->GetPatterns(levelInfo.currLevel, sizeOfPackedPList);

                    stringBufferFileLocal->CloseArchiveMMAP();
                    delete stringBufferFileLocal;

                    totalPatterns += sizeOfPackedPList;

                    if (sizeOfPackedPList > 0 && stringBufferLocal != nullptr)
                    {
                        for (PListType z = 0; z < stringBufferLocal->size(); z++)
                        {
                            if (finalMetaDataMap.find((*stringBufferLocal)[z]) != finalMetaDataMap.end())
                            {
                                patternsThatCantBeDumped.push_back((*stringBufferLocal)[z]);
                                //Set to this value to break out of for loop
                                earlyWriteIndex = static_cast<int>(fileNamesBackup.size()) - prevCurrentFile;
                                break;
                            }
                        }
                        stringBufferLocal->clear();
                        delete stringBufferLocal;
                    }

                }

                PListType patternsToDumpCount = totalPatterns - static_cast<PListType>(patternsThatCantBeDumped.size());
                stringstream sizeDifference;
                patternsThatCantBeDumped.unique();

                //NO MATCHES SO WE DUMP ALL PATTERNS TO FILE
                if (patternsThatCantBeDumped.size() == 0)
                {
                    //thread files
                    PListArchive* currChunkFile = nullptr;
                    bool notBegun = true;

                    auto iterator = finalMetaDataMap.begin();
                    while (iterator != finalMetaDataMap.end())
                    {

                        if (notBegun)
                        {
                            notBegun = false;
                            if (currChunkFile != nullptr)
                            {
                                currChunkFile->WriteArchiveMapMMAP(vector<PListType>(), "", true);
                                currChunkFile->CloseArchiveMMAP();
                                delete currChunkFile;
                            }

                            
                            stringstream fileNameForPrevList;
                            fileNameForPrevList << "PListChunks" << chunkFactorio->GenerateUniqueID();

                            newFileNames.push_back(fileNameForPrevList.str());

                            currChunkFile = new PListArchive(fileNameForPrevList.str(), true);
                           
                        }

                        if (iterator->second->size() >= _config.minOccurrence)
                        {
                            currChunkFile->WriteArchiveMapMMAP(*iterator->second);
                            interimCount++;
                            if (_config.processInts && (*iterator->second)[0] % sizeof(unsigned int) == 0) {
                                levelCount++;
                            }
                            else if (!_config.processInts) {
                                levelCount++;
                            }
                            _stats.SetMostCommonPattern(levelInfo.currLevel, static_cast<PListType>(iterator->second->size()), (*iterator->second)[0] - levelInfo.currLevel);

                            //Record level statistics
                            _stats.Lock();
                            _stats.SetTotalOccurrenceFrequency(levelInfo.currLevel, static_cast<PListType>(iterator->second->size()));

                            //If levelToOutput is not selected but -Pall is set or if -Pall is set and -Plevel is set to output data only for a specific level
                            if (_config.levelToOutput == 0 || _config.levelToOutput == levelInfo.currLevel)
                            {
                                PListType distances = 0;
                                PListType index = 0;
                                auto length = iterator->second->size();
                                PListType coverageSubtraction = 0;
                                //Calculate average distance between pattern instances
                                for (auto i = index; i < index + length - 1; i++)
                                {
                                    distances += (*iterator->second)[i + 1] - (*iterator->second)[i];
                                    if ((*iterator->second)[i + 1] - (*iterator->second)[i] < levelInfo.currLevel)
                                    {
                                        coverageSubtraction += levelInfo.currLevel - ((*iterator->second)[i + 1] - (*iterator->second)[i]);
                                    }
                                }

                                float averageDistance = ((float)distances) / ((float)(length - 1));
                                stringstream data;

                                //Struct used to contain detailed pattern information for one level
                                ProcessorStats::DisplayStruct outputData;
                                outputData.patternInstances = static_cast<PListType>(length);
                                outputData.patternCoveragePercentage = (float)100.0f*(((length*levelInfo.currLevel) - coverageSubtraction)) / (float)_config.currentFile->fileStringSize;
                                outputData.averagePatternDistance = averageDistance;
                                outputData.firstIndexToPattern = (*iterator->second)[index];

                                //If pnoname is not selected then strings are written to log, this could be for reasons where patterns are very long
                                if (!_config.suppressStringOutput)
                                {
                                    outputData.pattern = _config.currentFile->fileString.substr((*iterator->second)[index] - levelInfo.currLevel, levelInfo.currLevel);
                                }
                                _stats.detailedLevelInfo.push_back(outputData);
                            }

                            _stats.UnLock();

                        }
                        else
                        {
                            internalRemovedCount++;
                        }

                        delete iterator->second;
                        iterator = finalMetaDataMap.erase(iterator);
                    }

                    if (currChunkFile != nullptr)
                    {
                        currChunkFile->WriteArchiveMapMMAP(vector<PListType>(), "", true);
                        if (currChunkFile->mappingIndex != 0)
                        {
                            currChunkFile->CloseArchiveMMAP();
                            delete currChunkFile;
                        }
                        else
                        {
                            currChunkFile->CloseArchiveMMAP();
                            delete currChunkFile;
                            chunkFactorio->DeletePartialPatternFile(newFileNames[newFileNames.size() - 1], ARCHIVE_FOLDER);
                            newFileNames.pop_back();
                        }
                    }
                    finalMetaDataMap.clear();
                }
                foundAHit = false;

                std::string::size_type i = archiveCollection[a]->fileName.find(".txt");
                std::string::size_type begin = archiveCollection[a]->fileName.find("P");
                string tempString = archiveCollection[a]->fileName;
                tempString.erase(i);
                tempString.erase(0, begin);

                fileNameForLater.append(tempString);
                fileName = fileNameForLater;
                fileName.append("Patterns");

                std::string::size_type j = archiveCollection[a]->fileName.find("_");
                string copyString = archiveCollection[a]->fileName;
                copyString.erase(0, j + 1);
                std::string::size_type k = copyString.find(".txt");
                copyString.erase(k, 4);
                std::string::size_type sz;   // alias of size_t
                PListType sizeOfPackedPList = static_cast<PListType>(std::stoll(copyString, &sz));
                stringBufferFile = new PListArchive(fileName);
                stringBuffer = stringBufferFile->GetPatterns(levelInfo.currLevel, sizeOfPackedPList);
                packedPListSize = sizeOfPackedPList;

                //If the remaining files have patterns that are contained in them that are contained in the final map then we need to hold onto those patterns for later
                if (finalMetaDataMap.size() > 0)
                {
                    if (sizeOfPackedPList > 0)
                    {
                        for (PListType z = 0; z < stringBuffer->size(); z++)
                        {
                            if (finalMetaDataMap.find((*stringBuffer)[z]) != finalMetaDataMap.end())
                            {
                                foundAHit = true;
                                break;
                            }
                        }
                    }
                }
                else
                {
                    foundAHit = true;
                }

                if (foundAHit)
                {
                    archiveCollection[a]->GetPListArchiveMMAP(packedPListArray); //Needs MB
                }

            }

            PListType countAdded = 0;
            //Write all patterns to file except the ones that have a presence in the remaining partial files
            if (foundAHit)
            {
                for (PListType partialLists = 0; partialLists < packedPListArray.size(); partialLists++)
                {
                    try
                    {
                        //This allows us to back fill the others iterations when this didn't have a pattern
                        if (finalMetaDataMap.find((*stringBuffer)[partialLists]) == finalMetaDataMap.end())
                        {
                            if (!memoryOverflow)
                            {
                                finalMetaDataMap[(*stringBuffer)[partialLists]] = new vector<PListType>(packedPListArray[partialLists]->begin(), packedPListArray[partialLists]->end());
                                delete packedPListArray[partialLists];
                                packedPListArray[partialLists] = nullptr;
                                countAdded++;
                            }
                        }
                        else
                        {
                            finalMetaDataMap[(*stringBuffer)[partialLists]]->insert(finalMetaDataMap[(*stringBuffer)[partialLists]]->end(), packedPListArray[partialLists]->begin(), packedPListArray[partialLists]->end());
                            delete packedPListArray[partialLists];
                            packedPListArray[partialLists] = nullptr;
                            countAdded++;
                        }

                    }
                    catch (exception e)
                    {
                        cout << "System exception: " << e.what() << endl;
                    }
                }
            }


            if (foundAHit)
            {
                archiveCollection[a]->CloseArchiveMMAP();
                stringBufferFile->CloseArchiveMMAP();

                chunkFactorio->DeletePartialPatternFile(fileNameForLater, ARCHIVE_FOLDER);

                delete archiveCollection[a];
                delete stringBufferFile;


                PListType newCount = packedPListSize - countAdded;
                if (newCount > 0)
                {
                    stringstream namer;
                    namer << "PListChunks" << chunkFactorio->GenerateUniqueID() << "_" << newCount;

                    fileNamesBackup[prevCurrentFile + a] = namer.str();

                    PListType testCount = 0;
                    PListArchive* archiveCollective = nullptr;

                    archiveCollective = new PListArchive(fileNamesBackup[prevCurrentFile + a], true);

                    for (PListType partialLists = 0; partialLists < packedPListSize; partialLists++)
                    {
                        if (packedPListArray[partialLists] != nullptr)
                        {
                            archiveCollective->WriteArchiveMapMMAP(*packedPListArray[partialLists], (*stringBuffer)[partialLists]);
                            delete packedPListArray[partialLists];
                            packedPListArray[partialLists] = nullptr;
                            testCount++;
                        }
                    }
                    archiveCollective->DumpPatternsToDisk(levelInfo.currLevel);
                    archiveCollective->WriteArchiveMapMMAP(vector<PListType>(), "", true);
                    archiveCollective->CloseArchiveMMAP();

                    delete archiveCollective;
                }
                if (stringBuffer != nullptr)
                {
                    stringBuffer->clear();
                    delete stringBuffer;
                }
            }
            else
            {

                archiveCollection[a]->CloseArchiveMMAP();

                delete archiveCollection[a];

                stringBufferFile->CloseArchiveMMAP();

                delete stringBufferFile;

                stringBuffer->clear();
                delete stringBuffer;
            }
            if (!memoryOverflow || countAdded == packedPListSize || packedPListSize == 0)
            {
                currentFile++;
            }
        }

        //Start writing the patterns in pattern map to disk for complete pattern pictures
        PListArchive* currChunkFile = nullptr;
        bool notBegun = true;

        for (auto iterator = finalMetaDataMap.begin(); iterator != finalMetaDataMap.end(); iterator++)
        {
            if (notBegun)
            {
                notBegun = false;
                if (currChunkFile != nullptr)
                {
                    currChunkFile->WriteArchiveMapMMAP(vector<PListType>(), "", true);
                    currChunkFile->CloseArchiveMMAP();
                    delete currChunkFile;
                }
                stringstream fileNameForPrevList;
                fileNameForPrevList << "PListChunks" << chunkFactorio->GenerateUniqueID();

                newFileNames.push_back(fileNameForPrevList.str());

                currChunkFile = new PListArchive(fileNameForPrevList.str(), true);
                
            }

            if (iterator->second->size() >= _config.minOccurrence)
            {
                //Record level statistics
                _stats.Lock();

                currChunkFile->WriteArchiveMapMMAP(*iterator->second);

                interimCount++;
                if (_config.processInts && (*iterator->second)[0] % sizeof(unsigned int) == 0) {
                    levelCount++;
                }
                else if (!_config.processInts) {
                    levelCount++;
                }

                _stats.SetMostCommonPattern(levelInfo.currLevel, static_cast<PListType>(iterator->second->size()), (*iterator->second)[0] - levelInfo.currLevel);

                _stats.SetTotalOccurrenceFrequency(levelInfo.currLevel, static_cast<PListType>(iterator->second->size()));
                _stats.SetTotalOccurrenceFrequency(levelInfo.currLevel, static_cast<PListType>(iterator->second->size()));

                //If levelToOutput is not selected but -Pall is set or if -Pall is set and -Plevel is set to output data only for a specific level
                if (_config.levelToOutput == 0 || _config.levelToOutput == levelInfo.currLevel)
                {
                    PListType index = 0;
                    auto length = iterator->second->size();
                    PListType coverageSubtraction = 0;
                    PListType distances = 0;
                    std::stringstream stream;
                    //Calculate average distance between pattern instances
                    for (auto i = index; i < index + length - 1; i++)
                    {
                        PListType indexPattern;
                        if (_config.processInts)
                        {
                            indexPattern = (((*iterator->second)[i] / 4) - levelInfo.currLevel / 4);
                            stream << indexPattern << ",";
                            if ((i + 1) == (index + length - 1))
                            {
                                PListType indexlast;
                                indexlast = (((*iterator->second)[i + 1] / 4) - levelInfo.currLevel / 4);
                                stream << indexlast << ",";
                            }
                        }
                        else
                        {
                            indexPattern = ((*iterator->second)[i] - levelInfo.currLevel);
                            stream << indexPattern << ",";
                            if ((i + 1) == (index + length - 1))
                            {
                                PListType indexlast;
                                indexlast = ((*iterator->second)[i + 1] - levelInfo.currLevel);
                                stream << indexlast << ",";
                            }
                        }

                        distances += (*iterator->second)[i + 1] - (*iterator->second)[i];
                        if ((*iterator->second)[i + 1] - (*iterator->second)[i] < levelInfo.currLevel)
                        {
                            coverageSubtraction += levelInfo.currLevel - ((*iterator->second)[i + 1] - (*iterator->second)[i]);
                        }
                    }
                    stream << std::endl;
                    _stats.WriteValidationFile(stream.str());

                    float averageDistance = ((float)distances) / ((float)(length - 1));
                    stringstream data;

                    //Struct used to contain detailed pattern information for one level
                    ProcessorStats::DisplayStruct outputData;
                    outputData.patternInstances = static_cast<PListType>(length);
                    outputData.patternCoveragePercentage = (float)100.0f*(((length*levelInfo.currLevel) - coverageSubtraction)) / (float)_config.currentFile->fileStringSize;
                    outputData.averagePatternDistance = averageDistance;
                    outputData.firstIndexToPattern = (*iterator->second)[index] - levelInfo.currLevel;

                    //If pnoname is not selected then strings are written to log, this could be for reasons where patterns are very long
                    if (!_config.suppressStringOutput)
                    {
                        outputData.pattern = _config.currentFile->fileString.substr((*iterator->second)[index] - levelInfo.currLevel, levelInfo.currLevel);
                    }
                    _stats.detailedLevelInfo.push_back(outputData);
                }
                _stats.UnLock();
            }
            else
            {
                internalRemovedCount++;
            }

            delete iterator->second;
        }

        if (currChunkFile != nullptr)
        {
            currChunkFile->WriteArchiveMapMMAP(vector<PListType>(), "", true);
            if (currChunkFile->mappingIndex != 0)
            {
                currChunkFile->CloseArchiveMMAP();
                delete currChunkFile;
            }
            else
            {
                currChunkFile->CloseArchiveMMAP();
                delete currChunkFile;
                chunkFactorio->DeletePartialPatternFile(newFileNames[newFileNames.size() - 1], ARCHIVE_FOLDER);
                newFileNames.pop_back();
            }
        }

        //Delete the partial pattern files because they are no longer needed
        for (int a = prevCurrentFile; a < currentFile; a++)
        {
            chunkFactorio->DeletePartialPatternFile(fileNamesBackup[a], ARCHIVE_FOLDER);
        }
        prevCurrentFile = currentFile;
    }

    //Record level statistics
    _stats.Lock();

    _stats.SetEradicationsPerLevel(levelInfo.currLevel, _stats.GetEradicationsPerLevel(levelInfo.currLevel) + internalRemovedCount);
    _stats.SetEradicatedPatterns(_stats.GetEradicatedPatterns() + internalRemovedCount);

    if (_config.processInts && _config.levelToOutput % sizeof(unsigned int) == 0) {
        _stats.SetLevelRecording(levelInfo.currLevel, _stats.GetLevelRecording(levelInfo.currLevel) + levelCount);
    }
    else {
        _stats.SetLevelRecording(levelInfo.currLevel, _stats.GetLevelRecording(levelInfo.currLevel) + levelCount);
    }

    _stats.SetCurrentLevel(levelInfo.threadIndex, levelInfo.currLevel + 1);
    _stats.UnLock();

    return interimCount;
}

bool DiskProc::SplitUpWork(PListType newPatternCount, 
    bool& morePatternsToFind, 
    vector<string> fileList, 
    LevelPackage levelInfo)
{
    bool dispatchedNewThreads = false;
    bool alreadyUnlocked = false;
    _threadMgr.Lock();

    int unusedCores = (_config.numThreads - _threadMgr.getUnusedCores()) + 1;
    if (static_cast<int>(newPatternCount) < unusedCores && unusedCores > 1)
    {
        unusedCores = (int)newPatternCount;
    }
    //Need to have an available core, need to still have patterns to search and need to have more than 1 pattern to be worth splitting up the work
    if (unusedCores > 1 && morePatternsToFind && newPatternCount > 1)
    {
        bool spawnThreads = true;
        //If this thread is at the lowest level of progress spawn new threads
        if (spawnThreads)
        {
            vector<string> tempList = fileList;
            vector<vector<string>> balancedTruncList = BalanceWork(unusedCores, levelInfo, tempList);
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

                    dispatchedNewThreads = true;

                    LevelPackage levelInfoRecursion;
                    levelInfoRecursion.currLevel = levelInfo.currLevel;
                    levelInfoRecursion.threadIndex = levelInfo.threadIndex;
                    levelInfoRecursion.inceptionLevelLOL = levelInfo.inceptionLevelLOL + 1;
                    levelInfoRecursion.useRAM = false;

                    _threadMgr.defunctThread();

                    vector<future<void>> *localThreadPool = new vector<future<void>>();
                    for (PListType i = 0; i < localWorkingThreads.size(); i++)
                    {
                        _threadMgr.dispatchThread();

                        //Need to use this data type because std::async implements fake variadic templates
                        //and can only support 6 arguments...fake!
                        DataBundle bundle = { _config, _stats, _threadMgr };
                        //Lambda thread call
                        localThreadPool->push_back(std::async([]
                        (DiskProc * proc) {
                            proc->Process();
                        }, new DiskProc(balancedTruncList[i], bundle, _levelInfo)));
                    }
                    _threadMgr.UnLock();

                    alreadyUnlocked = true;
                    WaitOnThreads(localWorkingThreads, 
                        localThreadPool, 
                        true, 
                        levelInfo.currLevel, 
                        _threadMgr);

                    localThreadPool->erase(localThreadPool->begin(), localThreadPool->end());
                    (*localThreadPool).clear();
                    delete localThreadPool;
                    morePatternsToFind = false;
                }
            }
        }
    }
    if (!alreadyUnlocked)
    {
        _threadMgr.UnLock();
    }
    return dispatchedNewThreads;
}

vector<vector<string>> DiskProc::BalanceWork(unsigned int threadsToDispatch, 
    LevelPackage levelInfo, 
    vector<string> prevFileNames)
{
    ChunkFactory* chunkFactorio = ChunkFactory::instance();
    vector<vector<string>> newFileList;
    vector<PListArchive*> threadFiles;
    stringstream threadFilesNames;
    unsigned int threadNumber = 0;
    newFileList.resize(threadsToDispatch);
    if (prevFileNames.size() >= threadsToDispatch)
    {

        //Not distributing the pattern filess
        for (int a = 0; a < prevFileNames.size(); a++)
        {
            newFileList[threadNumber].push_back(prevFileNames[a]);

            //Increment chunk
            threadNumber++;
            if (threadNumber >= threadsToDispatch)
            {
                threadNumber = 0;
            }
        }
    }
    else
    {
        //Distributing files
        for (unsigned int a = 0; a < threadsToDispatch; a++)
        {
            threadFilesNames.str("");
            threadFilesNames << "PListChunks" << chunkFactorio->GenerateUniqueID();

            threadFiles.push_back(new PListArchive(threadFilesNames.str(), true));
            newFileList[a].push_back(threadFilesNames.str());

        }

        //Read in all pattern data from file and create new pattern files to evenly distribute pattern data to the threads
        for (PListType prevChunkCount = 0; prevChunkCount < prevFileNames.size(); prevChunkCount++)
        {
            PListArchive archive(prevFileNames[prevChunkCount]);

            while (archive.Exists() && !archive.IsEndOfFile())
            {
                //Just use 100 GB to say we want the whole file for now
                vector<vector<PListType>*> packedPListArray;
                archive.GetPListArchiveMMAP(packedPListArray);

                for (PListType prevIndex = 0; prevIndex < packedPListArray.size(); prevIndex++)
                {
                    threadFiles[threadNumber]->WriteArchiveMapMMAP(*(packedPListArray[prevIndex]));
                    delete packedPListArray[prevIndex];

                    //Increment chunk
                    threadNumber++;
                    if (threadNumber >= threadsToDispatch)
                    {
                        threadNumber = 0;
                    }
                }

                packedPListArray.erase(packedPListArray.begin(), packedPListArray.end());
            }
            archive.CloseArchiveMMAP();
            //Now delete it
            chunkFactorio->DeletePatternFile(prevFileNames[prevChunkCount], ARCHIVE_FOLDER);

        }

        //Flush to disk and close file handles
        for (unsigned int a = 0; a < threadsToDispatch; a++)
        {
            threadFiles[a]->WriteArchiveMapMMAP(vector<PListType>(), "", true);
            threadFiles[a]->CloseArchiveMMAP();
            delete threadFiles[a];
        }

    }

    return newFileList;
}
