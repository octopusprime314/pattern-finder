/*
* Copyright (c) 2016, 2017
* The University of Rhode Island
* All Rights Reserved
*
* This code is part of the Pattern Finder.
*
* Permission is granted to use, copy, create derivative works and
* redistribute this software and such derivative works for any
* purpose, so long as the copyright notice above, this grant of
* permission, and the disclaimer below appear in all copies made; and
* so long as the name of The University of Rhode Island is not used in
* any advertising or publicity pertaining to the use or distribution
* of this software without specific, written prior authorization.
*
* THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION FROM THE
* UNIVERSITY OF RHODE ISLAND AS TO ITS FITNESS FOR ANY PURPOSE, AND
* WITHOUT WARRANTY BY THE UNIVERSITY OF RHODE ISLAND OF ANY KIND, EITHER
* EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE. THE UNIVERSITY OF RHODE ISLAND SHALL NOT BE
* LIABLE FOR ANY DAMAGES, INCLUDING DIRECT, SPECIAL, INDIRECT,
* INCIDENTAL, OR CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM
* ARISING OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
* IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF SUCH
* DAMAGES.
*
* Author: Peter J. Morley
*
*/

#include "Forest.h"
#include "MemoryUtils.h"
#include <locale>
#include <list>
#include <algorithm>
#include <signal.h>
#include <numeric>
#include <iterator>

bool Forest::overMemoryCount = false;


vector<size_t> sort_indexes(const vector<ProcessorStats::DisplayStruct> &v) {

    // initialize original index locations
    vector<size_t> idx(v.size());
    iota(idx.begin(), idx.end(), 0);

    // sort indexes based on comparing values in v
    sort(idx.begin(), idx.end(),
        [&v](size_t i1, size_t i2) {return v[i1].patternInstances > v[i2].patternInstances; });

    return idx;
}

bool containsFile(string directory)
{
    bool foundFiles = false;
#if defined(_WIN64) || defined(_WIN32)
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir(directory.c_str())) != nullptr)
    {
        Logger::WriteLog("Files to be processed: \n");
        /* print all the files and directories within directory */
        while ((ent = readdir(dir)) != nullptr)
        {
            if (*ent->d_name)
            {
                string fileName = string(ent->d_name);

                if (!fileName.empty() && fileName.find("PList") != string::npos)
                {
                    foundFiles = true;
                    break;
                }
            }
        }
        closedir(dir);
    }
    else
    {
        //cout << "Problem reading from directory!" << endl;
    }
#elif defined(__linux__)
    DIR *dir;
    struct dirent *entry;

    if (!(dir = opendir(directory.c_str())))
        return false;

    while ((entry = readdir(dir)) != nullptr)
    {
        string fileName = string(entry->d_name);

        if (!fileName.empty() && fileName.find("PList") != string::npos)
        {
            foundFiles = true;
            break;
        }
    }
    closedir(dir);
#endif		
    return foundFiles;
}

void Forest::inteTochar(FileReader * files)
{
    std::string tempString;
    tempString.resize(files->fileStringSize);
    files->copyBuffer->read(&tempString[0], files->fileStringSize);

    size_t intEndPosition = tempString.find_first_of(","); //Find first instancee of a comma because data is comma seperated
    size_t stringIndex = 0;
    while (intEndPosition != std::string::npos) {

        std::string value = tempString.substr(stringIndex, intEndPosition - stringIndex);
        unsigned int data = std::stoul(value); //parse the numerical string to an unsigned int
        //Convert to from string representation of a number to unsigned int and then finally back to a 4 byte string
        files->fileString.push_back((data >> 24) & 0xFF);
        files->fileString.push_back((data >> 16) & 0xFF);
        files->fileString.push_back((data >> 8) & 0xFF);
        files->fileString.push_back((data) & 0xFF);

        stringIndex = intEndPosition + 1;
        intEndPosition = tempString.find_first_of(",", stringIndex); //Find first instancee of a comma because data is comma seperated
    }

    files->fileStringSize = static_cast<PListType>(files->fileString.size());

    files->fileName += "tmp";

    std::ofstream output = ofstream(files->fileName, ios::binary);

    output.write(files->fileString.c_str(), files->fileString.length());

    output.close();
}

Forest::Forest(int argc, char **argv)
{

    if (config.usingPureHD || (!config.usingPureHD && !config.usingPureHD))
    {
#if defined(_WIN64) || defined(_WIN32)
        //Hard code page size to 2 MB for windows
        PListArchive::hdSectorSize = 2097152;
        if (containsFile("..\\..\\log"))
        {
            system("del ..\\..\\log\\PList*.txt");
        }
#elif defined(__linux__)
        PListArchive::hdSectorSize = sysconf(_SC_PAGESIZE);
        if (containsFile("../log"))
        {
            system("rm -r ../log/PList*");
        }
#endif
    }

    PListArchive::totalLoops = PListArchive::hdSectorSize / sizeof(PListType);
    PListArchive::writeSize = PListArchive::hdSectorSize / 8;

    writingFlag = false;

    threadsDispatched = 0;
    threadsDefuncted = 0;
    mostMemoryOverflow = 0;
    currMemoryOverflow = 0;

    firstLevelProcessedHD = false;

    chunkFactorio = ChunkFactory::instance();


    config = ProcessorConfig::GetConfig(argc, argv);

    config.memoryUsageAtInception = MemoryUtils::GetProgramMemoryConsumption();
    MemoryUsedPriorToThread = MemoryUtils::GetProgramMemoryConsumption();
    overMemoryCount = false;
    processingFinished = false;

    countMutex = new mutex();
    prevPListArray = new vector<vector<PListType>*>();

    thread *memoryQueryThread = nullptr;

    double threadMemoryConsumptionInMB = MemoryUtils::GetProgramMemoryConsumption();
    Logger::WriteLog("Errant memory after processing level " , threadMemoryConsumptionInMB - config.memoryUsageAtInception, " in MB!\n");

    vector<map<int, double>> threadMap;

    threadPool = new vector<future<void>>();


    //Search through a list of files or an individual file
    for (auto iterator : config.files)
    {
        config.currentFile = iterator;

        const char * c = config.currentFile->fileName.c_str();

        // Open the file for the shortest time possible.

        config.currentFile->copyBuffer = new ifstream(c, ios::binary);

        config.currentFile->fileString.clear();
        if (config.processInts)
        {
            inteTochar(config.currentFile);

        }
        //If file is not opened then delete copy buffer and discard this file and continue processing with the next file
        if (!config.currentFile->copyBuffer->is_open())
        {
            //Close file handle once and for all
            config.currentFile->copyBuffer->clear();
            config.currentFile->fileString.clear();
            config.currentFile->fileString.reserve(0);

            delete config.currentFile;
            continue;
        }

        //If the file has not been read in and we are processing only with ram then read in the entire file
        if (config.currentFile->fileString.size() == 0 && config.usingPureRAM)
        {
            // config.currentFile->fileString.clear();

            if (!config.processInts)

            {
                config.currentFile->fileString.resize(config.currentFile->fileStringSize);
                config.currentFile->copyBuffer->read(&config.currentFile->fileString[0], config.currentFile->fileString.size());
            }
        }

        //Thread runs based on if the user wants to do throughput testing otherwise test iterations is set to 1
        for (unsigned int threadIteration = 0; threadIteration <= config.testIterations; threadIteration = config.numThreads)
        {
            //Divide between file load and previous level pLists and leave some for new pattern creation
            PListType memDivisor = (PListType)((config.memoryPerThread * 1000000) / 3.0f);

            std::string::size_type i = config.currentFile->fileName.find(DATA_FOLDER);
            string nameage = config.currentFile->fileName;
            if (i != std::string::npos)
                nameage.erase(i, sizeof(DATA_FOLDER) - 1);

            Logger::WriteLog("\n" , Logger::GetTime() , " File processing starting for: " , nameage, "\n");
            
            //Start the processing timer to keep track of how long processing takes
            initTime.Start();

            prevFileNameList.clear();
            newFileNameList.clear();
            prevFileNameList.resize(config.numThreads);
            newFileNameList.resize(config.numThreads);

            fileChunks.clear();
            fileChunks.reserve(0);

            chunkIndexToFileChunk.clear();

            //Kick off thread that processes how much memory the program uses at a certain interval
            memoryQueryThread = new thread(&Forest::MemoryQuery, this);

            //Initialize all possible values for the first list to nullptr
            prevPListArray->resize(256 * config.numThreads);
            for (unsigned int i = 0; i < 256 * config.numThreads; i++)
            {
                (*prevPListArray)[i] = nullptr;
            }

            stats.SetThreadStatistics(config.numThreads);

            activeThreads.resize(config.numThreads);
            for (unsigned int i = 0; i < config.numThreads; i++)
            {
                activeThreads[i] = false;
            }

            cout << "Memory that can be used per thread: " << config.memoryPerThread << " MB." << endl;

            PListType overallFilePosition = 0;

            //make this value 1 so calculations work correctly then reset
            LevelPackage levelInfo;
            levelInfo.currLevel = 1;
            levelInfo.inceptionLevelLOL = 0;
            levelInfo.threadIndex = 0;
            levelInfo.useRAM = stats.GetUsingRAM(0);
            levelInfo.coreIndex = 0;
            bool prediction = MemoryUtils::DiskOrSysMemProcessing(levelInfo, 1, config.currentFile->fileStringSize, config, stats);
            firstLevelProcessedHD = prediction;
            for (unsigned int i = 0; i < config.numThreads; i++)
            {
                stats.SetUsingRAM(i, !prediction);
                stats.SetCurrentLevel(i, 1);
            }

            //If processing with the hard drive then compute the number of passes needed to process
            PListType fileReadSize = memDivisor;
            PListType fileIterations = config.currentFile->fileStringSize / fileReadSize;
            if (config.currentFile->fileStringSize%fileReadSize != 0)
            {
                fileIterations++;
            }

            //If using pure ram or prediction say we can use ram then we don't care about memory constraints
            if (config.usingPureRAM || !prediction)
            {
                fileIterations = 1;
            }

            vector<string> backupFilenames;

            config.currentFile->copyBuffer->clear();
            config.currentFile->copyBuffer->seekg(0, ios::beg);

            //Only start processing time after file is read in
            StopWatch time;

            cout << "Number of threads processing file is " << config.numThreads << endl;

            //Number of iterations increase the lower the memory allowance is when using hard disk processing
            for (PListType z = 0; z < fileIterations; z++)
            {
                PListType position = 0;
                PListType patternCount = 0;
                if (config.currentFile->fileStringSize <= fileReadSize*z + fileReadSize)
                {
                    patternCount = config.currentFile->fileStringSize - fileReadSize*z;
                }
                else
                {
                    patternCount = fileReadSize;
                }

                //load in the entire file
                if (config.usingPureRAM || !prediction)
                {
                    patternCount = config.currentFile->fileStringSize;
                }

                if (!config.usingPureRAM)
                {
                    //new way to read in file

                    if (!config.processInts)
                    {
                        config.currentFile->fileString.clear();
                        config.currentFile->fileString.resize(patternCount);
                        config.currentFile->copyBuffer->read(&config.currentFile->fileString[0], config.currentFile->fileString.size());
                    }
                }

                //Calculate the number of indexes each thread will process
                PListType cycles = patternCount / config.numThreads;
                PListType lastCycle = patternCount - (cycles*config.numThreads);
                PListType span = cycles;

                for (unsigned int i = 0; i < config.numThreads; i++)
                {
                    if (!(i < config.numThreads - 1))
                    {
                        span = span + lastCycle;
                    }
                    //Dispatch either ram or hd processing based on earlier memory prediction
                    if (prediction)
                    {
                        threadPool->push_back(std::async(std::launch::async, &Forest::PlantTreeSeedThreadHD, this, overallFilePosition, position, span, i));
                    }
                    else
                    {
                        threadPool->push_back(std::async(std::launch::async, &Forest::PlantTreeSeedThreadRAM, this, overallFilePosition, position, span, i));
                    }
                    position += span;
                }

                PListType removedPatterns = 0;
                vector<unsigned int> localWorkingThreads;
                for (unsigned int i = 0; i < config.numThreads; i++)
                {
                    localWorkingThreads.push_back(i);
                }
                //Wait until level 1 threads have completed processing their portions of the file
                WaitForThreads(localWorkingThreads, threadPool);

                //If processing ram then all of the threads pattern vector must be pulled together
                if (!prediction)
                {
                    PListType indexOfList = 0;
                    std::map<string, PListType> countMap;
                    std::map<string, vector<PListType>> indexMap;

                    vector<PListType> pListLengths;
                    vector<PListType> linearList;
                    vector<vector<PListType>> consolodatedList(256);
                    int mostCommonPatternIndex;

                    for (PListType i = 0; i < prevPListArray->size(); i++)
                    {
                        if ((*prevPListArray)[i] != nullptr && (*prevPListArray)[i]->size() > 0)
                        {
                            countMap[config.currentFile->fileString.substr((*(*prevPListArray)[i])[0] - (levelInfo.currLevel), levelInfo.currLevel)] += static_cast<PListType>((*prevPListArray)[i]->size());

                            if (countMap[config.currentFile->fileString.substr((*(*prevPListArray)[i])[0] - (levelInfo.currLevel), levelInfo.currLevel)] > stats.GetMostCommonPatternCount(levelInfo.currLevel))
                            {
                                stats.SetMostCommonPattern(levelInfo.currLevel,
                                    countMap[config.currentFile->fileString.substr((*(*prevPListArray)[i])[0] - (levelInfo.currLevel), levelInfo.currLevel)],
                                    (*(*prevPListArray)[i])[0] - (levelInfo.currLevel));
                                mostCommonPatternIndex = i;
                                indexOfList = i;
                            }


                            consolodatedList[i / config.numThreads].insert(consolodatedList[i / config.numThreads].end(), (*prevPListArray)[i]->begin(), (*prevPListArray)[i]->end());


                            if ((*prevPListArray)[i]->size() >= 1)
                            {
                                indexMap[config.currentFile->fileString.substr((*(*prevPListArray)[i])[0] - (levelInfo.currLevel), levelInfo.currLevel)].push_back(i);
                            }
                        }
                    }
                    for (map<string, vector<PListType>>::iterator it = indexMap.begin(); it != indexMap.end(); it++)
                    {
                        if (it->second.size() == 1 && (*prevPListArray)[it->second[0]]->size() == 1)
                        {
                            (*prevPListArray)[it->second[0]]->clear();
                            removedPatterns++;
                        }
                    }

                    for (auto pList : consolodatedList)
                    {
                        if (pList.size() > 1)
                        {
                            pListLengths.push_back(static_cast<PListType>(pList.size()));
                            linearList.insert(linearList.end(), pList.begin(), pList.end());
                            stats.SetTotalOccurrenceFrequency(levelInfo.currLevel, static_cast<PListType>(pList.size()));
                        }
                    }

                    //If levelToOutput is not selected but -Pall is set or if -Pall is set and -Plevel is set to output data only for a specific level
                    if (config.levelToOutput == 0 || config.levelToOutput == levelInfo.currLevel)
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
                            //Calculate average distance between pattern instances
                            for (auto i = index; i < index + length - 1; i++)
                            {
                                distances += linearList[i + 1] - linearList[i];
                            }
                            float averageDistance = ((float)distances) / ((float)(length - 1));
                            stringstream data;

                            //Struct used to contain detailed pattern information for one level
                            ProcessorStats::DisplayStruct outputData;
                            outputData.patternInstances = length;
                            outputData.patternCoveragePercentage = (float)100.0f*(length*levelInfo.currLevel) / (float)config.currentFile->fileStringSize;
                            outputData.averagePatternDistance = averageDistance;
                            outputData.firstIndexToPattern = linearList[index] - 1;

                            //If pnoname is not selected then strings are written to log, this could be for reasons where patterns are very long
                            if (!config.suppressStringOutput)
                            {
                                outputData.pattern = config.currentFile->fileString.substr(linearList[index] - levelInfo.currLevel, levelInfo.currLevel);
                            }
                            stats.detailedLevelInfo.push_back(outputData);
                        }
                    }
                    mostCommonPatternIndex /= config.numThreads;
                    PListType distances = 0;
                    for (PListType j = 0; j < consolodatedList[mostCommonPatternIndex].size() - 1; j++)
                    {
                        distances += consolodatedList[mostCommonPatternIndex][j + 1] - consolodatedList[mostCommonPatternIndex][j];
                    }
                    float averageDistance = ((float)distances) / ((float)(consolodatedList[mostCommonPatternIndex].size() - 1));
                    stats.SetDistance(levelInfo.currLevel, averageDistance);

                    stats.SetLevelRecording(levelInfo.currLevel, static_cast<PListType>(countMap.size() - removedPatterns));

                    //Coverage cannot overlap on the first level by definition
                    stats.SetCoverage(static_cast<PListType>(levelInfo.currLevel), ((float)(stats.GetMostCommonPatternCount(levelInfo.currLevel))) / ((float)(config.currentFile->fileStringSize)));

                    stats.SetEradicationsPerLevel(levelInfo.currLevel, stats.GetEradicationsPerLevel(levelInfo.currLevel) + removedPatterns);
                    stats.SetEradicatedPatterns(stats.GetEradicatedPatterns() + removedPatterns);

                }
                overallFilePosition += position;

                threadPool->erase(threadPool->begin(), threadPool->end());
                (*threadPool).clear();

                //If processing hd then all the files saved to the hard disk needed to be pulled together
                if (prediction)
                {
                    for (unsigned int z = 0; z < config.numThreads; z++)
                    {
                        for (size_t a = 0; a < newFileNameList[z].size(); a++)
                        {
                            backupFilenames.push_back(newFileNameList[z][a]);
                        }
                    }
                }

                for (int a = 0; a < newFileNameList.size(); a++)
                {
                    newFileNameList[a].clear();
                }

            }

            int currentFile = 0;
            bool memoryOverflow = false;
            vector<vector<string>> temp;
            temp.resize(config.numThreads);
            if (prediction)
            {
                if (config.levelToOutput == 0 || config.levelToOutput != 0)
                {
                    /*ThreadCoreMan threadMgr(config.numThreads);
                    bool defuncted = false;
                    LevelPackage testLevelInfo;
                    testLevelInfo.currLevel = 1;
                    testLevelInfo.threadIndex = 0;
                    testLevelInfo.inceptionLevelLOL = 0;
                    testLevelInfo.useRAM = !prediction;
                    testLevelInfo.coreIndex = 0;
                    Proc::DataBundle data = { config, stats, threadMgr };
                    DiskProc diskProc(temp[0], data, testLevelInfo);
                    diskProc.ProcessChunksAndGenerate(backupFilenames, temp[0], memDivisor, 0, 1, true);*/

                    //All the partial pattern files generated need to be pulled together into one coherent pattern file
                    ProcessChunksAndGenerateLargeFile(backupFilenames, temp[0], memDivisor, 0, 1, true);
                }
            }

            ThreadCoreMan threadMgr(config.numThreads);
            bool defuncted = false;
            LevelPackage testLevelInfo;
            testLevelInfo.currLevel = 2;
            testLevelInfo.threadIndex = 0;
            testLevelInfo.inceptionLevelLOL = 0;
            testLevelInfo.useRAM = !prediction;
            testLevelInfo.coreIndex = 0;
            testLevelInfo.prevLevelProcType = prediction;
            Proc::DataBundle data = { config, stats, threadMgr };

            prediction = MemoryUtils::DiskOrSysMemProcessing(testLevelInfo, stats.GetLevelRecording(1), config.currentFile->fileStringSize, config, stats);

            //Instead of breaking, build a disk processing object and start searching
            PatternData* patterns = new PatternData();
            //Build up the files for processing using hd
            PrepDataFirstLevel(prediction, temp, patterns);

            //use that one prediction
            if (!prediction)
            {
                if (patterns->size() > 1) {
                    prevPListArray = patterns;
                }
                //Balance patterns across threads
                vector<vector<PListType>> balancedTruncList = ProcessThreadsWorkLoadRAMFirstLevel(config.numThreads, prevPListArray);
                vector<unsigned int> localWorkingThreads;
                for (unsigned int i = 0; i < balancedTruncList.size(); i++)
                {
                    activeThreads[i] = true;
                    localWorkingThreads.push_back(i);
                }

                countMutex->lock();
                for (unsigned int i = 0; i < localWorkingThreads.size(); i++)
                {
                    //Spawn each ram processing thread with level state information
                    threadsDispatched++;

                    PatternData* patternData = new PatternData();
                    for (auto index : balancedTruncList[i]) {
                        patternData->push_back((*prevPListArray)[index]);
                    }
                    threadPool->push_back(std::async([](SysMemProc * proc) {
                        proc->Process();
                    }, new SysMemProc(patternData, data, testLevelInfo)));

                }
                countMutex->unlock();
                WaitForThreads(localWorkingThreads, threadPool);
            }
            //HD processing
            else
            {
                //get ready for distribution
                vector<string> files;
                for (int i = 0; i < temp.size(); i++)
                {
                    for (int j = 0; j < temp[i].size(); j++)
                    {
                        files.push_back(temp[i][j]);
                    }
                }
                //Balance pattern files across threads
                vector<vector<string>> balancedTruncList = ProcessThreadsWorkLoadHD(config.numThreads, testLevelInfo, files);
                vector<unsigned int> localWorkingThreads;
                for (unsigned int i = 0; i < balancedTruncList.size(); i++)
                {
                    activeThreads[i] = true;
                    localWorkingThreads.push_back(i);
                }
                countMutex->lock();
                for (unsigned int i = 0; i < localWorkingThreads.size(); i++)
                {
                    //Spawn each hd processing thread with level state information
                    
                    threadPool->push_back(std::async([]
                    (DiskProc * proc) {
                        proc->Process();
                    }, new DiskProc(balancedTruncList[i], data, testLevelInfo)));
                }
                countMutex->unlock();
                WaitForThreads(localWorkingThreads, threadPool);
            }

            //Processing is over and stop the time to display the processing time
            time.Stop();
            threadMap.push_back(map<int, double>());
            threadMap.back()[config.numThreads] = time.GetTime();
            processingTimes.push_back(threadMap.back()[config.numThreads]);
            time.Display();
            Logger::WriteLog(config.numThreads , " threads were used to process file" , "\n");

            //Load in the rest of the file if it has not been completely loaded
            if (config.currentFile->fileStringSize != config.currentFile->fileString.size())
            {
                config.currentFile->fileString.clear();
                config.currentFile->copyBuffer->seekg(0);
                config.currentFile->fileString.resize(config.currentFile->fileStringSize);
                config.currentFile->copyBuffer->read(&config.currentFile->fileString[0], config.currentFile->fileString.size());
            }

            int iterator = sizeof(unsigned char);
            if (config.processInts) {
                iterator = sizeof(unsigned int);
            }
            for (PListType j = iterator - 1; j < stats.GetLevelRecordingSize() && stats.GetLevelRecording(j + 1) != 0; j += iterator)
            {
                (*Logger::patternOutputFile) << "Level " << (j + 1) / iterator << std::endl;

                stringstream buff;
                //Only process byte data as integers and process every 4 bytes of data because that is how an integer is encoded
                //Also the position of a pattern has to be divisible by 4 because patterns can only be in segments of 4 bytes i.e. integer encoding again
                //if (!config.processInts) {
                buff << "unique patterns = " << stats.GetLevelRecording(j + 1) <<
                    ", average occurrence frequency = " << ((double)stats.GetTotalOccurrenceFrequency(j + 1)) / ((double)stats.GetLevelRecording(j + 1)) <<
                    ", frequency of top pattern: " << stats.GetMostCommonPatternCount(j + 1) << endl;
                //}
                (*Logger::patternOutputFile) << buff.str();

                if (config.levelToOutput == j + 1)
                {
                    vector<size_t> indexMap = sort_indexes(stats.detailedLevelInfo);
                    PListType distances = 0;
                    PListType number = 1;
                    stringstream buff;
                    string pattern = config.currentFile->fileString.substr(stats.GetMostCommonPatternIndex(j + 1), j + 1);
                    stringstream tempBuff;
                    for (PListType z = 0; z < stats.detailedLevelInfo.size() && z < config.minimumFrequency; z++)
                    {
                        //Struct used to contain detailed pattern information for one level
                        ProcessorStats::DisplayStruct outputData = stats.detailedLevelInfo[indexMap[z]];

                        if (config.suppressStringOutput)
                        {
                            buff << number << ". instances = " << outputData.patternInstances << ", coverage = " << outputData.patternCoveragePercentage << "%, average pattern distance = " << outputData.averagePatternDistance << ", first occurrence index = " << outputData.firstIndexToPattern << endl;
                        }
                        else
                        {
                            //Only process byte data as integers and process every 4 bytes of data because that is how an integer is encoded
                            //Also the position of a pattern has to be divisible by 4 because patterns can only be in segments of 4 bytes i.e. integer encoding again
                            if (config.processInts) {
                                if (outputData.firstIndexToPattern % iterator == 0) {

                                    buff << number << ". pattern = ";

                                    for (int it = 0; it < pattern.size(); it += iterator) {
                                        std::string pattern = outputData.pattern;
                                        unsigned int intData = 0;
                                        intData += static_cast<unsigned char>(pattern[it]) << 24;
                                        intData += static_cast<unsigned char>(pattern[it + 1]) << 16;
                                        intData += static_cast<unsigned char>(pattern[it + 2]) << 8;
                                        intData += static_cast<unsigned char>(pattern[it + 3]);

                                        buff << intData << ",";
                                    }

                                    buff << " instances = " << outputData.patternInstances << ", coverage = " << outputData.patternCoveragePercentage << "%, average pattern distance = " << outputData.averagePatternDistance/4 << ", first occurrence index = " << outputData.firstIndexToPattern/4 << endl;
                                    number++;
                                }
                            }
                            //Process as pure byte data
                            else {
                                buff << number << ". pattern = " << outputData.pattern << ", instances = " << outputData.patternInstances << ", coverage = " << outputData.patternCoveragePercentage << "%, average pattern distance = " << outputData.averagePatternDistance << ", first occurrence index = " << outputData.firstIndexToPattern << endl;
                                number++;
                            }
                        }

                    }
                   
                    (*Logger::patternOutputFile) << tempBuff.str();
                    (*Logger::patternOutputFile) << buff.str();
                }
            }

            //Linux logger fix
            (*Logger::patternOutputFile).close();


            //Save pattern to csv format for matlab post processing scripts
            Logger::fillPatternData(config.currentFile->fileString, stats.GetMostCommonPatternIndexVector(), stats.GetMostCommonPatternCountVector());
            Logger::fileCoverageCSV(stats.GetCoverageVector());

            finalPattern[stats.GetLevelRecordingSize()]++;

            prevPListArray->clear();

            //If doing throughput calculations then double the thread count and compute the same file
            if (config.findBestThreadNumber)
            {
                config.numThreads = (config.numThreads * 2);
            }

            Logger::WriteLog("File Size " , config.currentFile->fileStringSize , " and eliminated patterns " , stats.GetEradicatedPatterns() , "\n\n\n");

            //File processing validation to make sure every index is processed and eliminated 
            //If we aren't doing a deep search in levels then there isn't a need to check that pattern finder is properly functioning..it's impossible
            if (config.currentFile->fileStringSize != stats.GetEradicatedPatterns() && config.maximum == -1)
            {
                cout << "Houston we are not processing patterns properly!" << endl;
                Logger::WriteLog("Houston we are not processing patterns properly!");
                //exit(0);
            }

            //Clean up memory watch dog thread
            if (memoryQueryThread != nullptr)
            {
                processingFinished = true;
                memoryQueryThread->join();
                delete memoryQueryThread;
                memoryQueryThread = nullptr;
                processingFinished = false;
            }
            //Clear statistics for next file processing
            stats.ResetData();
        }

        //Log throughput data
        stringstream loggingIt;
        loggingIt.str("");
        std::string::size_type i = config.currentFile->fileName.find(DATA_FOLDER);
        string nameage = config.currentFile->fileName;
        if (i != std::string::npos)
            nameage.erase(i, sizeof(DATA_FOLDER) - 1);

        Logger::WriteLog("\n",  Logger::GetTime(), " Ended processing for: ", nameage);

        for (pair<uint32_t, double> threadTime : threadMap.back())
        {
			Logger::WriteLog("Thread " , threadTime.first , " processed for " , threadTime.second , " milliseconds!");
        }
        //Log the percentage of files in folder that have been processed so far
		Logger::WriteLog("File collection percentage completed: ", threadMap.size() * 100 / config.files.size(), "%\n");

        //Close file handle once and for all
        config.currentFile->copyBuffer->clear();
        config.currentFile->fileString.clear();
        config.currentFile->fileString.reserve(0);

        delete config.currentFile;

        if (config.findBestThreadNumber)
        {
            config.numThreads = 1;
        }

    }

    //Generate data for file collection processing
    Logger::generateTimeVsFileSizeCSV(processingTimes, config.fileSizes);
    //Generate most patterns found in a large data set
    Logger::generateFinalPatternVsCount(finalPattern);
    //Generate thread throughput data
    if (config.findBestThreadNumber)
    {
        Logger::generateThreadsVsThroughput(threadMap);
    }

    for (int i = 0; i < fileChunks.size(); i++)
    {
        fileChunks[i].resize(0);
        fileChunks[i].shrink_to_fit();
    }

    delete threadPool;

    delete countMutex;
    delete prevPListArray;

    initTime.Stop();
    initTime.Display();

    threadMemoryConsumptionInMB = MemoryUtils::GetProgramMemoryConsumption();
    Logger::WriteLog("Errant memory after processing level " , threadMemoryConsumptionInMB - config.memoryUsageAtInception, " in MB!\n");
    Logger::WriteLog("Most memory overflow was : " , mostMemoryOverflow , " MB\n");
}

Forest::~Forest()
{
}

void Forest::MemoryQuery()
{
    //Query OS with how much memory the program is using and exit if it has reached the memory limit
    StopWatch swTimer;
    swTimer.Start();
    PListType previousEradicatedPatterns = 0;
    unsigned int overMemoryCountCounter = 0;
    PListType memoryCeiling = (PListType)MemoryUtils::GetAvailableRAMMB() - 1000;
    while (!processingFinished)
    {
        this_thread::sleep_for(std::chrono::milliseconds(1));
        double memoryOverflow = 0;
        overMemoryCount = MemoryUtils::IsOverMemoryCount(MemoryUsedPriorToThread, (double)config.memoryBandwidthMB, memoryOverflow);

        //Keep track of over memory usage 
        if(overMemoryCount) {
            overMemoryCountCounter++;
        }
        //If memory has been overused for one second be nice and double memory size
        if(overMemoryCountCounter == 1000) {
            config.memoryBandwidthMB = 2 * config.memoryBandwidthMB;
            config.memoryPerThread = config.memoryBandwidthMB / config.numThreads;
            overMemoryCountCounter = 0;
            Logger::WriteLog("Memory limit now doubled to: " , config.memoryBandwidthMB , "\n");
        }

        currMemoryOverflow = memoryOverflow;
        if (mostMemoryOverflow < memoryOverflow)
        {
            mostMemoryOverflow = memoryOverflow;
        }
        //Abort mission and do not exit gracefully ie dump cause we will be pageing soon
        if (currMemoryOverflow + config.memoryBandwidthMB > memoryCeiling && !config.usingPureRAM)
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
            for (PListType j = 0; j < stats.GetCurrentLevelSize(); j++)
            {
				Logger::WriteLog("Thread " , j , " is at level: " , stats.GetCurrentLevel(j) , "\n");
            }
           
			Logger::WriteLog("Percentage of file processed is: " , (((double)stats.GetEradicatedPatterns()) / ((double)config.currentFile->fileStringSize))*100.0f , "%\n");
			Logger::WriteLog("Percentage of cpu usage: " , MemoryUtils::CPULoad() , "%\n");
			Logger::WriteLog("Approximate processing time left: " ,
				((((double)(config.currentFile->fileStringSize - stats.GetEradicatedPatterns()))  * timeStamp) 
					/ ((double)(stats.GetEradicatedPatterns() - previousEradicatedPatterns))) / 1000.0f , 
				" seconds\n");
            initTime.DisplayNow();

            previousEradicatedPatterns = stats.GetEradicatedPatterns();
        }
    }
}

void Forest::PrepDataFirstLevel(bool prediction, vector<vector<string>>& fileList, vector<vector<PListType>*>* prevLocalPListArray)
{
    vector<vector<string>> tempFileList = fileList;
    for (int a = 0; a < fileList.size(); a++)
    {
        fileList[a].clear();
    }

    //If we need to use the hard disk for processing the upcoming level
    if (prediction)
    {
        //If the previous level was processed using the hard disk then just distribute the pattern files across the threads
        if (!stats.GetUsingRAM(0))
        {
            //chunk files
            vector<PListArchive*> threadFiles;
            stringstream threadFilesNames;
            unsigned int threadNumber = 0;

            for (int a = 0; a < tempFileList.size(); a++)
            {
                for (int b = 0; b < tempFileList[a].size(); b++)
                {
                    fileList[threadNumber].push_back(tempFileList[a][b]);

                    //Increment chunk
                    threadNumber++;
                    if (threadNumber >= config.numThreads)
                    {
                        threadNumber = 0;
                    }
                }
            }
        }
        //If previous level was processing using only ram then write pattern vector data to the file system
        else if (stats.GetUsingRAM(0))
        {
            //chunk files
            vector<PListArchive*> threadFiles;
            stringstream threadFilesNames;
            unsigned int threadNumber = 0;

            for (unsigned int a = 0; a < config.numThreads; a++)
            {
                threadFilesNames.str("");
                threadFilesNames << "PListChunks" << chunkFactorio->GenerateUniqueID();

                threadFiles.push_back(new PListArchive(threadFilesNames.str(), true));
                fileList[a].push_back(threadFilesNames.str());
            }
            for (PListType prevIndex = 0; prevIndex < prevLocalPListArray->size(); )
            {
                list<PListType> *sorting = new list<PListType>();

                for (unsigned int threadCount = 0; threadCount < config.numThreads; threadCount++)
                {
                    if ((*prevLocalPListArray)[prevIndex] != nullptr)
                    {
                        copy((*prevLocalPListArray)[prevIndex]->begin(), (*prevLocalPListArray)[prevIndex]->end(), std::back_inserter(*sorting));
                        ((*prevLocalPListArray)[prevIndex])->erase(((*prevLocalPListArray)[prevIndex])->begin(), ((*prevLocalPListArray)[prevIndex])->end());
                        delete (*prevLocalPListArray)[prevIndex];
                        prevIndex++;
                    }
                }
                vector<PListType> finalVector;
                sorting->sort();
                std::copy(sorting->begin(), sorting->end(), std::back_inserter(finalVector));
                sorting->clear();
                delete sorting;

                threadFiles[threadNumber]->WriteArchiveMapMMAP(finalVector);
                threadFiles[threadNumber]->WriteArchiveMapMMAP(vector<PListType>(), "", true);


                //Increment chunk
                threadNumber++;
                if (threadNumber >= config.numThreads)
                {
                    threadNumber = 0;
                }
            }
            //Clear out the array also after deletion
            prevLocalPListArray->clear();

            for (unsigned int a = 0; a < config.numThreads; a++)
            {
                threadFiles[a]->WriteArchiveMapMMAP(vector<PListType>(), "", true);
                threadFiles[a]->CloseArchiveMMAP();
                delete threadFiles[a];
            }
        }
    }
    //If the upcoming level is going to be processed using ram
    else if (!prediction)
    {
        //If the previous level using hard disk to process then read in pattern file data and convert to pattern vectors
        if (!stats.GetUsingRAM(0))
        {
            prevLocalPListArray->clear();
            for (PListType i = 0; i < tempFileList.size(); i++)
            {
                for (PListType prevChunkCount = 0; prevChunkCount < tempFileList[i].size(); prevChunkCount++)
                {
                    PListArchive archive(tempFileList[i][prevChunkCount]);
                    while (archive.Exists() && !archive.IsEndOfFile())
                    {
                        //Just use 100 GB to say we want the whole file for now
                        vector<vector<PListType>*> packedPListArray;
                        archive.GetPListArchiveMMAP(packedPListArray);

                        prevLocalPListArray->push_back(new vector<PListType>());
                        for (auto pattern : packedPListArray) {
                            prevLocalPListArray->back()->insert(prevLocalPListArray->back()->end(), pattern->begin(), pattern->end());
                        }
                        packedPListArray.erase(packedPListArray.begin(), packedPListArray.end());
                    }
                    archive.CloseArchiveMMAP();
                }
            }

            for (PListType i = 0; i < config.numThreads; i++)
            {
                if (!config.history)
                {
                    chunkFactorio->DeletePatternFiles(tempFileList[i], ARCHIVE_FOLDER);
                }
            }
            //Transition to using entire file when first level was hard disk processing and next level is pure ram
            if (config.currentFile->fileString.size() != config.currentFile->fileStringSize)
            {
                //new way to read in file
                countMutex->lock();
                config.currentFile->copyBuffer->seekg(0);
                config.currentFile->fileString.resize(config.currentFile->fileStringSize);
                config.currentFile->copyBuffer->read(&config.currentFile->fileString[0], config.currentFile->fileString.size());
                countMutex->unlock();
            }
        }
    }

    //Set all the threads upcoming level processing mode
    for (unsigned int a = 0; a < config.numThreads; a++)
    {
        stats.SetUsingRAM(0, !prediction);
    }
}

void Forest::PrepData(bool prediction, LevelPackage& levelInfo, vector<string>& fileList, vector<vector<PListType>*>* prevLocalPListArray)
{
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
                list<PListType> *sorting = new list<PListType>();
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
            if (!config.history)
            {
                chunkFactorio->DeletePatternFiles(fileList, ARCHIVE_FOLDER);
            }
            fileList.clear();
        }
    }

    levelInfo.useRAM = !prediction;
    stats.SetUsingRAM(levelInfo.threadIndex, !prediction);

    //Read in all file data back in if the level is using only ram
    if (levelInfo.useRAM && config.currentFile->fileString.size() != config.currentFile->fileStringSize)
    {
        //new way to read in file
        countMutex->lock();
        config.currentFile->copyBuffer->seekg(0);
        config.currentFile->fileString.resize(config.currentFile->fileStringSize);
        config.currentFile->copyBuffer->read(&config.currentFile->fileString[0], config.currentFile->fileString.size());
        countMutex->unlock();
    }
}
void Forest::WaitForThreads(vector<unsigned int> localWorkingThreads, vector<future<void>> *localThreadPool, bool recursive, unsigned int level)
{
    PListType threadsFinished = 0;
    StopWatch oneSecondTimer;
    //Poll each processing thread to see if it is done processing and then release thread power to the other threads when a thread is released from proessing
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

                    countMutex->lock();
                    activeThreads[k] = false;
                    countMutex->unlock();

                    if (level != 0)
                    {
						Logger::WriteLog("Thread " , localWorkingThreads[k] , " finished all processing" , '\n');
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

vector<vector<string>> Forest::ProcessThreadsWorkLoadHD(unsigned int threadsToDispatch, LevelPackage levelInfo, vector<string> prevFileNames)
{

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

vector<vector<PListType>> Forest::ProcessThreadsWorkLoadRAMFirstLevel(unsigned int threadsToDispatch, vector<vector<PListType>*>* patterns)
{
    //Evenly distribute pattern data by breaking down the pattern vector and adding them to the thread processing pattern vectors for the first level
    vector<vector<PListType>> balancedList(threadsToDispatch);
    vector<PListType> balancedSizeList;
    for (PListType i = 0; i < threadsToDispatch; i++)
    {
        balancedSizeList.push_back(0);
    }

    if (patterns->size() == threadsToDispatch * 256)
    {
        vector<PListType> patternTotals(256, 0);
        for (PListType i = 0; i < 256; i++)
        {
            for (PListType z = 0; z < threadsToDispatch; z++)
            {
                patternTotals[i] += static_cast<PListType>((*patterns)[i*threadsToDispatch + z]->size());
            }
        }

        for (PListType i = 0; i < patternTotals.size(); i++)
        {
            bool found = false;
            PListType smallestIndex = 0;
            PListType smallestAmount = -1;
            for (PListType j = 0; j < threadsToDispatch; j++)
            {
                if ((*patterns)[(i*threadsToDispatch) + j] != nullptr)
                {
                    for (PListType z = 0; z < threadsToDispatch; z++)
                    {
                        if (balancedSizeList[z] < smallestAmount)
                        {
                            smallestAmount = balancedSizeList[z];
                            smallestIndex = z;
                            found = true;
                        }
                    }

                }
            }
            if (found)
            {
                balancedSizeList[smallestIndex] += patternTotals[i];
                for (PListType j = 0; j < threadsToDispatch; j++)
                {
                    balancedList[smallestIndex].push_back((i*threadsToDispatch) + j);
                }
            }
        }
    }
    else
    {
        vector<PListType> patternTotals;
        for (PListType i = 0; i < patterns->size(); i++)
        {
            patternTotals.push_back(static_cast<PListType>((*patterns)[i]->size()));
        }

        for (PListType i = 0; i < patternTotals.size(); i++)
        {
            bool found = false;
            PListType smallestIndex = 0;
            PListType smallestAmount = -1;

            if ((*patterns)[i] != nullptr)
            {
                for (PListType z = 0; z < threadsToDispatch; z++)
                {
                    if (balancedSizeList[z] < smallestAmount)
                    {
                        smallestAmount = balancedSizeList[z];
                        smallestIndex = z;
                        found = true;
                    }
                }
            }
            if (found)
            {
                balancedSizeList[smallestIndex] += patternTotals[i];
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

    vector<vector<PListType>> balancedTruncList(sizeToTruncate);
    PListType internalCount = 0;
    for (unsigned int i = 0; i < threadsToDispatch; i++)
    {
        if (balancedList[i].size() > 0)
        {
            balancedTruncList[internalCount] = balancedList[i];
            internalCount++;
        }
    }

    return balancedTruncList;
}

PListType Forest::ProcessChunksAndGenerateLargeFile(vector<string> fileNamesToReOpen, vector<string>& newFileNames, PListType memDivisor, unsigned int threadNum, unsigned int currLevel, bool firstLevel)
{
    //This is the same function as ProcessChunksAndGenerate but is specialized to process level 1 data faster
    int currentFile = 0;
    int prevCurrentFile = currentFile;
    bool memoryOverflow = false;
    PListType interimCount = 0;
    unsigned int threadNumber = 0;
    PListType removedPatterns = 0;

    PListType currPatternCount = 0;
    //Approximate pattern count for this level
    if (currLevel == 1)
    {
        currPatternCount = 256;
    }
    else
    {
        currPatternCount = 256 * stats.GetLevelRecording(currLevel);
    }
    
    std::vector<std::string> fileNames;
    map<string, PListArchive*> currChunkFiles;
    for (PListType a = 0; a < currPatternCount; a++)
    {
        stringstream fileNameage;
        stringstream fileNameForPrevList;
        PListType newID = chunkFactorio->GenerateUniqueID();
        fileNameage << ARCHIVE_FOLDER << "PListChunks" << newID << ".txt";
        fileNameForPrevList << "PListChunks" << newID;

        prevFileNameList[threadNumber].push_back(fileNameForPrevList.str());

        stringstream pattern;
        pattern << (char)a;
        currChunkFiles[pattern.str()] = new PListArchive(fileNameForPrevList.str(), true);
        fileNames.push_back(fileNameForPrevList.str());

        threadNumber++;
        threadNumber %= config.numThreads;
    }

    vector<string> fileNamesBackup;
    for (int a = 0; a < fileNamesToReOpen.size(); a++)
    {
        fileNamesBackup.push_back(fileNamesToReOpen[a]);
    }

    map<string, pair<PListType, PListType>> patternCounts;
    while (currentFile < fileNamesBackup.size())
    {
        memoryOverflow = false;

        vector<PListArchive*> archiveCollection;

        PListType distances[256] = { 0 };
        PListType coverageSubtraction[256] = { 0 };

        for (int a = 0; a < fileNamesBackup.size() - prevCurrentFile; a++)
        {

            archiveCollection.push_back(new PListArchive(fileNamesBackup[a + prevCurrentFile]));

            //Our job is to trust whoever made the previous chunk made it within the acceptable margin of error so we compensate by saying take up to double the size if the previous
            //chunk went a little over the allocation constraint

            vector<vector<PListType>*> packedPListArray;
            vector<string> *stringBuffer = nullptr;
            PListArchive *stringBufferFile = nullptr;
            string fileNameForLater = "";
            PListType packedPListSize = 0;
            string fileName = "";
            bool foundAHit = true;

            //Check for memory over usage
            if (overMemoryCount && !memoryOverflow)
            {
				Logger::WriteLog("Overflow at Process Chunks And Generate of " , currMemoryOverflow , " in MB!\n");
            }
            else if (overMemoryCount && memoryOverflow)
            {
				Logger::WriteLog("Overflow at Process Chunks And Generate of " , currMemoryOverflow , " in MB!\n");
            }


            archiveCollection[a]->GetPListArchiveMMAP(packedPListArray); //Needs MB
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
            std::string::size_type j = archiveCollection[a]->fileName.find("_");
            string copyString = archiveCollection[a]->fileName;
            copyString.erase(0, j + 1);
            std::string::size_type k = copyString.find(".txt");
            copyString.erase(k, 4);
            std::string::size_type sz;   // alias of size_t
            PListType sizeOfPackedPList = static_cast<PListType>(std::stoll(copyString, &sz));
            stringBuffer = stringBufferFile->GetPatterns(currLevel, packedPListSize);

            PListType countAdded = 0;
            //Write all patterns contained in the pattern map to complete pattern files
            if (foundAHit)
            {
                for (PListType partialLists = 0; partialLists < packedPListArray.size(); partialLists++)
                {
                    try
                    {

                        if (overMemoryCount && !memoryOverflow)
                        {
                            Logger::WriteLog("Overflow at Process Chunks And Generate of " , currMemoryOverflow , " in MB!\n");
                        }

                        string pattern = (*stringBuffer)[partialLists];

                        if (patternCounts.find(pattern) != patternCounts.end())
                        {
                            patternCounts[pattern].first += static_cast<PListType>(packedPListArray[partialLists]->size());
                        }
                        else
                        {
                            patternCounts[pattern].first = static_cast<PListType>(packedPListArray[partialLists]->size());
                            patternCounts[pattern].second = (*(packedPListArray[partialLists]))[0];
                        }

                        unsigned char val = (unsigned char)pattern[0];
                        for (int i = 0; i < packedPListArray[partialLists]->size() - 1; i++)
                        {
                            distances[val] += (*(packedPListArray[partialLists]))[i + 1] - (*(packedPListArray[partialLists]))[i];
                            if ((*(packedPListArray[partialLists]))[i + 1] - (*(packedPListArray[partialLists]))[i] < currLevel)
                            {
                                coverageSubtraction[val] += currLevel - ((*(packedPListArray[partialLists]))[i + 1] - (*(packedPListArray[partialLists]))[i]);
                            }
                        }

                        currChunkFiles[pattern]->WriteArchiveMapMMAP(*(packedPListArray[partialLists]));
                        delete packedPListArray[partialLists];
                        packedPListArray[partialLists] = nullptr;

                    }
                    catch (exception e)
                    {
                        cout << "System exception: " << e.what() << endl;
                    }
                }
            }

            archiveCollection[a]->CloseArchiveMMAP();
            stringBufferFile->CloseArchiveMMAP();

            //Delete partial files after processed
            chunkFactorio->DeletePartialPatternFile(fileNameForLater, ARCHIVE_FOLDER);

            delete archiveCollection[a];


            if (stringBuffer != nullptr)
            {
                stringBuffer->clear();
                delete stringBuffer;
            }

            delete stringBufferFile;

            if (!memoryOverflow)
            {
                currentFile++;
            }
        }

        //Record level 1 statistics
        for (PListType a = 0; a < currPatternCount; a++)
        {
            stringstream buff;
            buff << (char)a;

          

            currChunkFiles[buff.str()]->WriteArchiveMapMMAP(vector<PListType>(), "", true);

            string fileName = currChunkFiles[buff.str()]->patternName;
            currChunkFiles[buff.str()]->CloseArchiveMMAP();
            delete currChunkFiles[buff.str()];

            currChunkFiles[buff.str()] = new PListArchive(fileName);

          

            //Just use 100 GB to say we want the whole file for now
            vector<vector<PListType>*> packedPListArray;
            currChunkFiles[buff.str()]->GetPListArchiveMMAP(packedPListArray);

            vector<PListType>* patternsRolledUp = new std::vector<PListType>();
            for (auto pattern : packedPListArray) {
                patternsRolledUp->insert(patternsRolledUp->end(), pattern->begin(), pattern->end());
            }
            currChunkFiles[buff.str()]->CloseArchiveMMAP();
            chunkFactorio->DeletePartialPatternFile(currChunkFiles[buff.str()]->patternName, ARCHIVE_FOLDER);
            delete currChunkFiles[buff.str()];

            currChunkFiles[buff.str()] = new PListArchive(fileNames[a], true);

            currChunkFiles[buff.str()]->WriteArchiveMapMMAP(*patternsRolledUp);
            currChunkFiles[buff.str()]->WriteArchiveMapMMAP(vector<PListType>(), "", true);

            //chunkFactorio->DeletePartialPatternFile(fileToDelete, ARCHIVE_FOLDER);





            bool empty = true;
            PListType patterCount = (currChunkFiles[buff.str()]->prevMappingIndex / sizeof(PListType)) - sizeof(PListType);

            //Keeps track of the index in pListLengths vector
            vector<PListType> positionsInLinearList;

            if (currChunkFiles[buff.str()]->mappingIndex > (2 * sizeof(PListType)))
            {
                empty = false;
                interimCount++;
                stats.SetMostCommonPattern(currLevel, patternCounts[buff.str()].first, patternCounts[buff.str()].second - currLevel);

                stats.SetTotalOccurrenceFrequency(currLevel, patternCounts[buff.str()].first);

                //If levelToOutput is not selected but -Pall is set or if -Pall is set and -Plevel is set to output data only for a specific level
                if (config.levelToOutput == 0 || config.levelToOutput == currLevel)
                {
                    PListType length = patternCounts[buff.str()].first;

                    float averageDistance = ((float)distances[a]) / ((float)(length - 1));
                    stringstream data;

                    //Struct used to contain detailed pattern information for one level
                    ProcessorStats::DisplayStruct outputData;
                    outputData.patternInstances = length;
                    outputData.patternCoveragePercentage = (float)100.0f*(((length*currLevel) - coverageSubtraction[a])) / (float)config.currentFile->fileStringSize;
                    outputData.averagePatternDistance = averageDistance;
                    outputData.firstIndexToPattern = patternCounts[buff.str()].second - currLevel + 1;

                    //If pnoname is not selected then strings are written to log, this could be for reasons where patterns are very long
                    if (!config.suppressStringOutput)
                    {
                        outputData.pattern = config.currentFile->fileString.substr(patternCounts[buff.str()].second - currLevel, currLevel);
                    }
                    stats.detailedLevelInfo.push_back(outputData);
                }
                newFileNames.push_back(fileNames[a]);

            }
            else if (currChunkFiles[buff.str()]->mappingIndex == (2 * sizeof(PListType)))
            {
                removedPatterns++;
            }

            string fileToDelete = currChunkFiles[buff.str()]->patternName;
            currChunkFiles[buff.str()]->CloseArchiveMMAP();
            delete currChunkFiles[buff.str()];

            if (fileNamesBackup.size() == currentFile && empty)
            {
                chunkFactorio->DeletePartialPatternFile(fileToDelete, ARCHIVE_FOLDER);
            }
        }
    }

    countMutex->lock();

    stats.SetEradicationsPerLevel(currLevel, stats.GetEradicationsPerLevel(currLevel) + removedPatterns);
    stats.SetEradicatedPatterns(stats.GetEradicatedPatterns() + removedPatterns);

    stats.SetLevelRecording(currLevel, stats.GetLevelRecording(currLevel) + interimCount);
    stats.SetCoverage(currLevel, ((float)(interimCount)) / ((float)config.currentFile->fileStringSize));
    stats.SetCurrentLevel(threadNum, currLevel + 1);

    countMutex->unlock();

    return interimCount;
}
void Forest::PlantTreeSeedThreadHD(PListType positionInFile, PListType startPatternIndex, PListType numPatternsToSearch, unsigned int threadNum)
{
    LevelPackage levelInfo;
    levelInfo.currLevel = 1;
    levelInfo.coreIndex = threadNum;
    levelInfo.threadIndex = threadNum;

    //Populate vector with 256 entries for the first level processing which can only have up to 256 patterns
    //and make an early approximation for vector sizes to prevent as much reallocations as possible
    PListType earlyApproximation = static_cast<PListType>(config.currentFile->fileString.size() / (256 * (config.numThreads)));
    vector<vector<PListType>*> leaves(256);
    for (int i = 0; i < 256; i++)
    {
        leaves[i] = new vector<PListType>();
        leaves[i]->reserve(earlyApproximation);
    }

    PListType counting = 0;

    for (PListType i = startPatternIndex; i < numPatternsToSearch + startPatternIndex; i++)
    {
        //Index into the file plus an offset if processing file in partitions
        int temp = i + positionInFile + 1;
        uint8_t tempIndex = (uint8_t)config.currentFile->fileString[i];
        if (config.patternToSearchFor.size() == 0 || config.currentFile->fileString[i] == config.patternToSearchFor[0])
        {
            //Only search in memory locations that are divisible by four to prevent incorrect patterns
            //that could exist between integer encodings
            if (config.processInts && (i + positionInFile) % 4 == 0) {
                leaves[tempIndex]->push_back(temp);

            }

            else if (!config.processInts) {	
                leaves[tempIndex]->push_back(temp);

            }     
            counting++;
        }
        //If RAM memory is allocated over the limit then dump the patterns to file
        if (overMemoryCount && counting >= PListArchive::hdSectorSize)
        {
            stringstream stringBuilder;
            PListType newID = chunkFactorio->GenerateUniqueID();
            stringBuilder << newID;
            newFileNameList[threadNum].push_back(chunkFactorio->CreatePartialPatternFile(stringBuilder.str(), leaves, levelInfo));
            //Reallocate vectors for storage
            for (int i = 0; i < 256; i++)
            {
                leaves[i] = new vector<PListType>();
                leaves[i]->reserve(earlyApproximation);
            }
        }
    }

    //Push patterns to file from whatever is left of processing
    stringstream stringBuilder;
    PListType newID = chunkFactorio->GenerateUniqueID();
    stringBuilder << newID;
    newFileNameList[threadNum].push_back(chunkFactorio->CreatePartialPatternFile(stringBuilder.str(), leaves, levelInfo));
}

void Forest::PlantTreeSeedThreadRAM(PListType positionInFile, PListType startPatternIndex, PListType numPatternsToSearch, PListType threadIndex)
{
    //Populate vector with 256 entries for the first level processing which can only have up to 256 patterns
    vector<PListType>* leaves[256];
    for (int i = 0; i < 256; i++)
    {
        leaves[i] = new vector<PListType>();
    }

    //Last index in the file that will be processed, creating a range to search in
    PListType endPatternIndex = numPatternsToSearch + startPatternIndex;

    for (PListType i = startPatternIndex; i < endPatternIndex; i++)
    {
        //Index into the file plus an offset if processing file in partitions
        int temp = i + positionInFile + 1;
        uint8_t tempIndex = (uint8_t)config.currentFile->fileString[i];

        //Only search in memory locations that are divisible by four to prevent incorrect patterns
        //that could exist between integer encodings
        if (config.processInts && (i + positionInFile) % 4 == 0) {
            leaves[tempIndex]->push_back(temp);
        }

        else if (!config.processInts) {
            //If ranges are set only search for patterns in the specified range
            if (config.lowRange == config.highRange || tempIndex >= config.lowRange && tempIndex <= config.highRange)
            {
                leaves[tempIndex]->push_back(temp);
            }
        }
    }

    //Push new patterns into the shared vector that will be pulled together with the other processing threads
    for (int i = 0; i < 256; i++)
    {
        (*prevPListArray)[threadIndex + i*config.numThreads] = leaves[i];
    }

    //Set level to 2 because level 1 processing is finished
    stats.SetCurrentLevel(threadIndex, 2);
}