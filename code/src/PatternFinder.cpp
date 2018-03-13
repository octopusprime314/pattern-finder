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

#include "PatternFinder.h"
#include "MemoryUtils.h"
#include "FileProcessor.h"
#include <numeric>
#include <algorithm>

//Loose sorting function that will need to be placed in a class soon!
vector<size_t> sortIndexes(const vector<ProcessorStats::DisplayStruct> &v) {

    // initialize original index locations
    vector<size_t> idx(v.size());
    iota(idx.begin(), idx.end(), 0);

    // sort indexes based on comparing values in v
    sort(idx.begin(), idx.end(),
        [&v](size_t i1, size_t i2) {return v[i1].patternInstances > v[i2].patternInstances; });

    return idx;
}

PatternFinder::PatternFinder(int argc, char **argv)
{
    ConfigurationParams config = ProcessorConfig::GetConfig(argc, argv);
    ProcessorStats stats;

    if (config.usingPureHD || (!config.usingPureHD && !config.usingPureHD))
    {
#if defined(_WIN64) || defined(_WIN32)
        //Hard code page size to 2 MB for windows
        PListArchive::hdSectorSize = 2097152;
        if (FileReader::containsFile("..\\..\\log"))
        {
            system("del ..\\..\\log\\PList*.txt");
        }
#elif defined(__linux__)
        PListArchive::hdSectorSize = sysconf(_SC_PAGESIZE);
        if (FileReader::containsFile("../log"))
        {
            system("rm -r ../log/PList*");
        }
#endif
    }

    PListArchive::totalLoops = PListArchive::hdSectorSize / sizeof(PListType);
    PListArchive::writeSize = PListArchive::hdSectorSize / 8;

    int level = 0;
    if (config.processInts)
    {
        level = config.levelToOutput / 4;
    }
    else
    {
        level = config.levelToOutput;
    }
    stats.OpenValidationFile("level" + std::to_string(level) + "outIndex.txt");

    config.memoryUsageAtInception = MemoryUtils::GetProgramMemoryConsumption();
   
    vector<map<int, double>> threadMap;
    map<PListType, PListType> finalPattern;

    //File statistics
    vector<double> processingTimes;

    vector<future<void>> *threadPool = new vector<future<void>>();

    //Search through a list of files or an individual file
    for (auto iterator : config.files)
    {
        config.currentFile = iterator;

        StopWatch time;

        FileProcessor* test = new FileProcessor(iterator, config, stats);

        //Processing is over and stop the time to display the processing time
        time.Stop();
        threadMap.push_back(map<int, double>());
        threadMap.back()[config.numThreads] = time.GetTime();
        processingTimes.push_back(threadMap.back()[config.numThreads]);
        time.Display();
        Logger::WriteLog(config.numThreads, " threads were used to process file", "\n");

        //Load in the rest of the file if it has not been completely loaded
        if (config.currentFile->fileStringSize != config.currentFile->fileString.size())
        {
            config.currentFile->fileString.clear();
            config.currentFile->copyBuffer->seekg(0);
            config.currentFile->fileString.resize(config.currentFile->fileStringSize);
            config.currentFile->copyBuffer->read(&config.currentFile->fileString[0], config.currentFile->fileString.size());
        }

        int patternLength = sizeof(unsigned char);
        if (config.processInts) {
            patternLength = sizeof(unsigned int);
        }

        auto levels = stats.GetLevelRecordingSize();
        for (PListType j = patternLength - 1; j < levels && stats.GetLevelRecording(j + 1) != 0; j += patternLength)
        {
            (*Logger::patternOutputFile) << "Level " << (j + 1) / patternLength << std::endl;

            stringstream buff;
            buff << "unique patterns = " << stats.GetLevelRecording(j + 1) 
                << ", average occurrence frequency = " 
                << ((double)stats.GetTotalOccurrenceFrequency(j + 1)) / ((double)stats.GetLevelRecording(j + 1)) 
                << ", frequency of top pattern: " << stats.GetMostCommonPatternCount(j + 1) 
                << endl;
            (*Logger::patternOutputFile) << buff.str();

            if (config.levelToOutput == j + 1)
            {
                vector<size_t> indexMap = sortIndexes(stats.detailedLevelInfo);
                PListType number = 1;
                stringstream output;
                string pattern = config.currentFile->fileString.substr(stats.GetMostCommonPatternIndex(j + 1), j + 1);
                stringstream tempBuff;
                for (PListType z = 0; z < stats.detailedLevelInfo.size() && z < config.minimumFrequency; z++)
                {
                    //Struct used to contain detailed pattern information for one level
                    ProcessorStats::DisplayStruct outputData = stats.detailedLevelInfo[indexMap[z]];

                    if (config.suppressStringOutput)
                    {
                        output << number << ". instances = " << outputData.patternInstances
                            << ", coverage = " << outputData.patternCoveragePercentage 
                            << "%, average pattern distance = " << outputData.averagePatternDistance 
                            << ", first occurrence index = " << outputData.firstIndexToPattern 
                            << endl;
                    }
                    else
                    {
                        //Only process byte data as integers and process every 4 bytes of data because that is how an integer is encoded
                        //Also the position of a pattern has to be divisible by 4 because patterns can only be in segments of 4 bytes i.e. integer encoding again
                        if (config.processInts) {
                            if (outputData.firstIndexToPattern % patternLength == 0) {

                                output << number << ". pattern = ";

                                for (int it = 0; it < pattern.size(); it += patternLength) {
                                    std::string patternToConvert = outputData.pattern;
                                    unsigned int intData = 0;
                                    intData += static_cast<unsigned char>(patternToConvert[it]) << 24;
                                    intData += static_cast<unsigned char>(patternToConvert[it + 1]) << 16;
                                    intData += static_cast<unsigned char>(patternToConvert[it + 2]) << 8;
                                    intData += static_cast<unsigned char>(patternToConvert[it + 3]);

                                    output << intData << ",";
                                }

                                output << " instances = " << outputData.patternInstances
                                    << ", coverage = " << outputData.patternCoveragePercentage 
                                    << "%, average pattern distance = " << outputData.averagePatternDistance / 4 
                                    << ", first occurrence index = " << outputData.firstIndexToPattern / 4 
                                    << endl;
                                number++;
                            }
                        }
                        //Process as pure byte data
                        else {
                            output << number << ". pattern = " << outputData.pattern
                                << ", instances = " << outputData.patternInstances 
                                << ", coverage = " << outputData.patternCoveragePercentage 
                                << "%, average pattern distance = " << outputData.averagePatternDistance 
                                << ", first occurrence index = " << outputData.firstIndexToPattern 
                                << endl;
                            number++;
                        }
                    }
                }
                (*Logger::patternOutputFile) << tempBuff.str();
                (*Logger::patternOutputFile) << output.str();
            }
        }

        //Linux logger fix
        (*Logger::patternOutputFile).close();


        //Save pattern to csv format for matlab post processing scripts
        //Logger::fillPatternData(config.currentFile->fileString, stats.GetMostCommonPatternIndexVector(), stats.GetMostCommonPatternCountVector());
        //Logger::fileCoverageCSV(stats.GetCoverageVector());

        finalPattern[stats.GetLevelRecordingSize()]++;

        Logger::WriteLog("File Size ", config.currentFile->fileStringSize, 
            " and eliminated patterns ", stats.GetEradicatedPatterns());

        //File processing validation to make sure every index is processed and eliminated 
        //If we aren't doing a deep search in levels then there isn't a need to check that pattern finder is properly functioning..it's impossible
        if (config.processInts)
        {
            if (config.currentFile->fileStringSize / 4 != stats.GetEradicatedPatterns() && config.maximum == -1)
            {
                Logger::WriteLog("Houston we are NOT processing Integer patterns properly!");
            }
            else
            {
                Logger::WriteLog("Houston we are processing Integer patterns properly!");
            }
        }
        else
        {
            if (config.currentFile->fileStringSize != stats.GetEradicatedPatterns() && config.maximum == -1)
            {
                Logger::WriteLog("Houston we are NOT processing patterns properly!");
            }
            else
            {
                Logger::WriteLog("Houston we are processing patterns properly!");
            }
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

    Logger::WriteLog("\n", Logger::GetTime(), " Ended processing for: ", nameage);

    for (pair<uint32_t, double> threadTime : threadMap.back())
    {
        Logger::WriteLog("Thread ", threadTime.first, " processed for ", threadTime.second, " milliseconds!");
    }
    //Log the percentage of files in folder that have been processed so far
    Logger::WriteLog("File collection percentage completed: ", threadMap.size() * 100 / config.files.size(), "%");

    //Close file handle once and for all
    config.currentFile->copyBuffer->clear();
    config.currentFile->fileString.clear();
    config.currentFile->fileString.reserve(0);

    delete config.currentFile;


    

    //Generate data for file collection processing
    Logger::generateTimeVsFileSizeCSV(processingTimes, config.fileSizes);
    //Generate most patterns found in a large data set
    Logger::generateFinalPatternVsCount(finalPattern);
   
    delete threadPool;
    stats.CloseValidationFile();
}

PatternFinder::~PatternFinder()
{

}