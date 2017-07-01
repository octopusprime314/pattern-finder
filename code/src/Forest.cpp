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

bool Forest::overMemoryCount = false;


vector<size_t> sort_indexes(const vector<ProcessorStats::DisplayStruct> &v) {

  // initialize original index locations
  vector<size_t> idx(v.size());
  iota(idx.begin(), idx.end(), 0);

  // sort indexes based on comparing values in v
  sort(idx.begin(), idx.end(),
       [&v](size_t i1, size_t i2) {return v[i1].patternInstances > v[i2].patternInstances;});

  return idx;
}

bool containsFile(string directory)
{
	bool foundFiles = false;
#if defined(_WIN64) || defined(_WIN32)
	DIR *dir;
	struct dirent *ent;
	if ((dir = opendir (directory.c_str())) != NULL) 
	{
		Logger::WriteLog("Files to be processed: \n");
		/* print all the files and directories within directory */
		while ((ent = readdir (dir)) != NULL) 
		{
			if(*ent->d_name)
			{
				string fileName = string(ent->d_name);

				if(!fileName.empty() && fileName.find("PList") != string::npos)
				{
					foundFiles = true;
					break;
				}
			}
		}
		closedir (dir);
	} else
	{
		//cout << "Problem reading from directory!" << endl;
	}
#elif defined(__linux__)
	DIR *dir;
	struct dirent *entry;

	if (!(dir = opendir(directory.c_str())))
		return false;

	while((entry = readdir (dir)) != NULL)
	{
		string fileName = string(entry->d_name);

		if(!fileName.empty() && fileName.find("PList") != string::npos)
		{
			foundFiles = true;
			break;
		}
	} 
	closedir(dir);
#endif		
	return foundFiles;
}

Forest::Forest(int argc, char **argv)
{

	if(config.usingPureHD || (!config.usingPureHD && !config.usingPureHD))
	{
#if defined(_WIN64) || defined(_WIN32)
		//Hard code page size to 2 MB for windows
		PListArchive::hdSectorSize = 2097152;
		if(containsFile("..\\..\\Log"))
		{
			system("del ..\\..\\Log\\PList*.txt");
		}
#elif defined(__linux__)
		PListArchive::hdSectorSize = sysconf (_SC_PAGESIZE);
		if(containsFile("../Log"))
		{
			system("rm -r ../Log/PList*");
		}
#endif
	}

	PListArchive::totalLoops = PListArchive::hdSectorSize/sizeof(PListType);
	PListArchive::writeSize = PListArchive::hdSectorSize/8;

	f = 0;
	writingFlag = false;

	threadsDispatched = 0;
	threadsDefuncted = 0;
	mostMemoryOverflow = 0;
	currMemoryOverflow = 0;
	
	firstLevelProcessedHD = false;

	chunkFactorio = ChunkFactory::instance();

	config = ProcessorConfig::GetConfig(argc, argv);

	MemoryUsageAtInception = MemoryUtils::GetProgramMemoryConsumption();
	MemoryUsedPriorToThread = MemoryUtils::GetProgramMemoryConsumption();
	overMemoryCount = false;
	processingFinished = false;

	countMutex = new mutex();
	prevPListArray = new vector<vector<PListType>*>();

	
	//main thread is a hardware thread so dispatch threads requested minus 1
	if(config.findBestThreadNumber)
	{
		config.numThreads = 1;
	}

	memoryCeiling = (PListType)MemoryUtils::GetAvailableRAMMB() - 1000;

	//If memory bandwidth not an input
	if(!config.usingMemoryBandwidth)
	{

		//Leave 1 GB to spare for operating system 
		config.memoryBandwidthMB = memoryCeiling - 1000;
	}

	thread *memoryQueryThread = NULL;
	
	double threadMemoryConsumptionInMB = MemoryUtils::GetProgramMemoryConsumption();
	stringstream crappy;
	crappy << "Errant memory after processing level " << threadMemoryConsumptionInMB - MemoryUsageAtInception << " in MB!\n";
	Logger::WriteLog(crappy.str());

	vector<map<int, double>> threadMap;
	
	threadPool = new vector<future<void>>();

	//Search through a list of files or an individual file
	for(f = 0; f < config.files.size(); f++)
	{
		const char * c = config.files[f]->fileName.c_str();
	
		// Open the file for the shortest time possible.

		config.files[f]->copyBuffer = new ifstream(c, ios::binary);

		//If file is not opened then delete copy buffer and discard this file and continue processing with the next file
		if (!config.files[f]->copyBuffer->is_open()) 
		{
			//Close file handle once and for all
			config.files[f]->copyBuffer->clear();
			config.files[f]->fileString.clear();
			config.files[f]->fileString.reserve(0);

			delete config.files[f];
			continue;
		}

		//If the file has not been read in and we are processing only with ram then read in the entire file
		if(config.files[f]->fileString.size() == 0 && config.usingPureRAM)
		{
			config.files[f]->fileString.clear();
			config.files[f]->fileString.resize(config.files[f]->fileStringSize);
			config.files[f]->copyBuffer->read( &config.files[f]->fileString[0], config.files[f]->fileString.size());
		}

		//Thread runs based on if the user wants to do throughput testing otherwise test iterations is set to 1
		for(unsigned int threadIteration = 0; threadIteration <= config.testIterations; threadIteration = config.numThreads)
		{

			stringstream loggingIt;
			loggingIt.str("");
			std::string::size_type i = config.files[f]->fileName.find(DATA_FOLDER);
			string nameage = config.files[f]->fileName;
			if (i != std::string::npos)
				nameage.erase(i, sizeof(DATA_FOLDER)-1);

			loggingIt << "\n" << Logger::GetTime() << " File processing starting for: " << nameage << endl << endl;
			Logger::WriteLog(loggingIt.str());
			cout << loggingIt.str();

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
			
			//Initialize all possible values for the first list to NULL
			prevPListArray->resize(256*config.numThreads);
			for(unsigned int i = 0; i < 256*config.numThreads; i++)
			{
				(*prevPListArray)[i] = NULL;
			}

			stats.SetThreadStatistics(config.numThreads);

			activeThreads.resize(config.numThreads);
			for(unsigned int i = 0; i < config.numThreads; i++)
			{
				activeThreads[i] = false;
			}

			memoryPerThread = config.memoryBandwidthMB/config.numThreads;
			cout << "Memory that can be used per thread: " << memoryPerThread << " MB." << endl;

			
			PListType overallFilePosition = 0;

			//make this value 1 so calculations work correctly then reset
			LevelPackage levelInfo;
			levelInfo.currLevel = 1;
			levelInfo.inceptionLevelLOL = 0;
			levelInfo.threadIndex = 0;
			levelInfo.useRAM = stats.GetUsingRAM(0);
			levelInfo.coreIndex = 0;
			bool prediction = PredictHardDiskOrRAMProcessing(levelInfo, 1, config.files[f]->fileStringSize);
			firstLevelProcessedHD = prediction;
			for(unsigned int i = 0; i < config.numThreads; i++)
			{
				stats.SetUsingRAM(i, !prediction);
				stats.SetCurrentLevel(i, 1);
			}

			//If processing with the hard drive then compute the number of passes needed to process
			PListType fileReadSize = (PListType)((memoryPerThread*1000000)/3.0f);
			PListType fileIterations = config.files[f]->fileStringSize/fileReadSize;
			if(config.files[f]->fileStringSize%fileReadSize != 0)
			{
				fileIterations++;
			}

			//If using pure ram or prediction say we can use ram then we don't care about memory constraints
			if(config.usingPureRAM || !prediction)
			{
				fileIterations = 1;
			}

			vector<string> backupFilenames;

			config.files[f]->copyBuffer->clear();
			config.files[f]->copyBuffer->seekg(0, ios::beg);

			//Only start processing time after file is read in
			StopWatch time;

			cout << "Number of threads processing file is " << config.numThreads << endl;

			//Number of iterations increase the lower the memory allowance is when using hard disk processing
			for(PListType z = 0; z < fileIterations; z++)
			{
				PListType position = 0;
				PListType patternCount = 0;
				if(config.files[f]->fileStringSize <= fileReadSize*z + fileReadSize)
				{
					patternCount = config.files[f]->fileStringSize - fileReadSize*z;
				}
				else
				{
					patternCount = fileReadSize;
				}

				//load in the entire file
				if(config.usingPureRAM || !prediction)
				{
					patternCount = config.files[f]->fileStringSize;
				}

				if(!config.usingPureRAM)
				{
					//new way to read in file
					config.files[f]->fileString.clear();
					config.files[f]->fileString.resize(patternCount);
					config.files[f]->copyBuffer->read( &config.files[f]->fileString[0], config.files[f]->fileString.size());
				}
				
				//Calculate the number of indexes each thread will process
				PListType cycles = patternCount/config.numThreads;
				PListType lastCycle = patternCount - (cycles*config.numThreads);
				PListType span = cycles;
				
				for(unsigned int i = 0; i < config.numThreads; i++)
				{
					if(!(i < config.numThreads - 1))
					{
						span = span + lastCycle;
					}	
					//Dispatch either ram or hd processing based on earlier memory prediction
					if(prediction)
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
				for(unsigned int i = 0; i < config.numThreads; i++)
				{
					localWorkingThreads.push_back(i);
				}
				//Wait until level 1 threads have completed processing their portions of the file
				WaitForThreads(localWorkingThreads, threadPool); 

				//If processing ram then all of the threads pattern vector must be pulled together
				if(!prediction)
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
						if((*prevPListArray)[i] != nullptr && (*prevPListArray)[i]->size() > 0)
						{
							countMap[config.files[f]->fileString.substr((*(*prevPListArray)[i])[0] - (levelInfo.currLevel), levelInfo.currLevel)] += static_cast<PListType>((*prevPListArray)[i]->size());
							
							if( countMap[config.files[f]->fileString.substr((*(*prevPListArray)[i])[0] - (levelInfo.currLevel), levelInfo.currLevel)] > stats.GetMostCommonPatternCount(levelInfo.currLevel))
							{
								stats.SetMostCommonPattern(levelInfo.currLevel, 
									countMap[config.files[f]->fileString.substr((*(*prevPListArray)[i])[0] - (levelInfo.currLevel), levelInfo.currLevel)],
									(*(*prevPListArray)[i])[0] - (levelInfo.currLevel));
								mostCommonPatternIndex = i;
								indexOfList = i;
							}

							
							consolodatedList[i / config.numThreads].insert(consolodatedList[i / config.numThreads].end(), (*prevPListArray)[i]->begin(), (*prevPListArray)[i]->end());
							

							if((*prevPListArray)[i]->size() >= 1)
							{
								indexMap[config.files[f]->fileString.substr((*(*prevPListArray)[i])[0] - (levelInfo.currLevel), levelInfo.currLevel)].push_back(i);
							}
						}
					}
					for(map<string, vector<PListType>>::iterator it = indexMap.begin(); it != indexMap.end(); it++)
					{
						if(it->second.size() == 1 && (*prevPListArray)[it->second[0]]->size() == 1)
						{
							(*prevPListArray)[it->second[0]]->clear();
							removedPatterns++;
						}
					}
					
					for( auto pList : consolodatedList)
					{
						if(pList.size() > 1)
						{
							pListLengths.push_back(pList.size());
							linearList.insert(linearList.end(), pList.begin(), pList.end());
							stats.SetTotalOccurrenceFrequency(levelInfo.currLevel, pList.size());
						}
					}

					//If levelToOutput is not selected but -Pall is set or if -Pall is set and -Plevel is set to output data only for a specific level
					if(config.levelToOutput == 0 || config.levelToOutput == levelInfo.currLevel)
					{

						//Keeps track of the index in pListLengths vector
						vector<PListType> positionsInLinearList(pListLengths.size());
						PListType pos = 0;
						for(PListType i = 0; i < positionsInLinearList.size(); i++)
						{
							positionsInLinearList[i] = pos;
							pos += pListLengths[i];
						}
						for(PListType z = 0; z < pListLengths.size(); z++)
						{
							PListType distances = 0;
							PListType index = positionsInLinearList[z];
							PListType length = pListLengths[z];
							//Calculate average distance between pattern instances
							for(int i = index; i < index + length - 1; i++)
							{
								distances += linearList[i+1] - linearList[i];
							}
							float averageDistance = ((float)distances)/((float)(length - 1));
							stringstream data;

							//Struct used to contain detailed pattern information for one level
							ProcessorStats::DisplayStruct outputData;
							outputData.patternInstances = length;
							outputData.patternCoveragePercentage = (float)100.0f*(length*levelInfo.currLevel)/(float)config.files[f]->fileStringSize;
							outputData.averagePatternDistance =  averageDistance;
							outputData.firstIndexToPattern = linearList[index];
				
							//If pnoname is not selected then strings are written to log, this could be for reasons where patterns are very long
							if(!config.suppressStringOutput)
							{
								outputData.pattern = config.files[f]->fileString.substr(linearList[index] - levelInfo.currLevel, levelInfo.currLevel);
							}
							stats.detailedLevelInfo.push_back(outputData);
						}
					}
					mostCommonPatternIndex /= config.numThreads;
					PListType distances = 0;
					for(PListType j = 0; j < consolodatedList[mostCommonPatternIndex].size() - 1; j++)
					{
						distances += consolodatedList[mostCommonPatternIndex][j+1] - consolodatedList[mostCommonPatternIndex][j];
					}
					float averageDistance = ((float)distances)/((float)(consolodatedList[mostCommonPatternIndex].size() - 1));
					stats.SetDistance(levelInfo.currLevel, averageDistance);
					
					stats.SetLevelRecording(levelInfo.currLevel, static_cast<PListType>(countMap.size() - removedPatterns));

					//Coverage cannot overlap on the first level by definition
					stats.SetCoverage(static_cast<PListType>(levelInfo.currLevel), ((float)(stats.GetMostCommonPatternCount(levelInfo.currLevel))) / ((float)(config.files[f]->fileStringSize)));
					
					stats.SetEradicationsPerLevel(levelInfo.currLevel, stats.GetEradicationsPerLevel(levelInfo.currLevel) + removedPatterns);
					stats.SetEradicatedPatterns(stats.GetEradicatedPatterns() + removedPatterns);

				}
				overallFilePosition += position;

				threadPool->erase(threadPool->begin(), threadPool->end());
				(*threadPool).clear();
				
				//If processing hd then all the files saved to the hard disk needed to be pulled together
				if(prediction)
				{
					for(unsigned int z = 0; z < config.numThreads; z++)
					{
						for(size_t a = 0; a < newFileNameList[z].size(); a++)
						{
							backupFilenames.push_back(newFileNameList[z][a]);
						}
					}
				}

				for(int a = 0; a < newFileNameList.size(); a++)
				{
					newFileNameList[a].clear();
				}

			}

			//Divide between file load and previous level pLists and leave some for new pattern creation
			PListType memDivisor = (PListType)((memoryPerThread*1000000)/3.0f);

			int currentFile = 0;
			bool memoryOverflow = false;
			vector<string> temp;
			if(prediction)
			{
				if(config.levelToOutput == 0 || config.levelToOutput != 0)
				{
					//All the partial pattern files generated need to be pulled together into one coherent pattern file
					ProcessChunksAndGenerateLargeFile(backupFilenames, temp, memDivisor, 0, 1, true);
				}
			}

			//Start searching for level 2 and beyond!
			if(2 <= config.maximum)
			{
				NextLevelTreeSearch(2);
			}

			//Processing is over and stop the time to display the processing time
			time.Stop();
			threadMap.push_back(map<int, double>());
			threadMap[f][config.numThreads] = time.GetTime();
			processingTimes.push_back(threadMap[f][config.numThreads]);
			time.Display();
			stringstream buffery;
			buffery << config.numThreads << " threads were used to process file" << endl;
			Logger::WriteLog(buffery.str());

			//Load in the rest of the file if it has not been completely loaded
			if(config.files[f]->fileStringSize != config.files[f]->fileString.size())
			{
				config.files[f]->fileString.clear();
				config.files[f]->copyBuffer->seekg( 0 );
				config.files[f]->fileString.resize(config.files[f]->fileStringSize);
				config.files[f]->copyBuffer->read( &config.files[f]->fileString[0], config.files[f]->fileString.size());
			}

			
			for(PListType j = 0; j < stats.GetLevelRecordingSize() && stats.GetLevelRecording(j + 1) != 0; j++)
			{
				(*Logger::patternOutputFile) << "Level " << j + 1 << std::endl;

				string pattern = config.files[f]->fileString.substr(stats.GetMostCommonPatternIndex(j + 1), j + 1);
					stringstream buff;
					buff << "unique patterns = " << stats.GetLevelRecording(j+1) << 
						", average occurrence frequency = " << ((double)stats.GetTotalOccurrenceFrequency(j+1))/((double)stats.GetLevelRecording(j+1)) << 
						", frequency of top pattern: " << stats.GetMostCommonPatternCount(j + 1) << endl;
					(*Logger::patternOutputFile) << buff.str();

				if(config.levelToOutput == j + 1)
				{
					vector<size_t> indexMap = sort_indexes(stats.detailedLevelInfo);
					PListType distances = 0;
					PListType number = 1;
					stringstream buff;
					string pattern = config.files[f]->fileString.substr(stats.GetMostCommonPatternIndex(j + 1), j + 1);
					for(PListType z = 0; z < stats.detailedLevelInfo.size() && z < config.minimumFrequency; z++)
					{
						//Struct used to contain detailed pattern information for one level
						ProcessorStats::DisplayStruct outputData = stats.detailedLevelInfo[indexMap[z]];
						
						buff.str("");
						if(config.suppressStringOutput)
						{
							buff << number << ". instances = " << outputData.patternInstances << ", coverage = " << outputData.patternCoveragePercentage << "%, average pattern distance = " << outputData.averagePatternDistance << ", first occurrence index = " << outputData.firstIndexToPattern << endl;
						}
						else
						{
							buff << number << ". pattern = " << outputData.pattern << ", instances = " << outputData.patternInstances << ", coverage = " << outputData.patternCoveragePercentage << "%, average pattern distance = " << outputData.averagePatternDistance << ", first occurrence index = " << outputData.firstIndexToPattern << endl;
						}
						(*Logger::patternOutputFile) << buff.str();
						number++;
					}
				}
			}
			

			//Save pattern to csv format for matlab post processing scripts
			Logger::fillPatternData(config.files[f]->fileString, stats.GetMostCommonPatternIndexVector(), stats.GetMostCommonPatternCountVector());
			Logger::fileCoverageCSV(stats.GetCoverageVector());

			finalPattern[stats.GetLevelRecordingSize()]++;

			//Clear out all memory from ram processing
			for (int i = 0; i < prevPListArray->size(); i++)
			{
				if((*prevPListArray)[i] != NULL)
				{
					delete (*prevPListArray)[i];
				}
			}
			prevPListArray->clear();

			//If doing throughput calculations then double the thread count and compute the same file
			if(config.findBestThreadNumber)
			{
				config.numThreads = (config.numThreads * 2);
			}

			
			crappy.str("");
			crappy << "File Size " << config.files[f]->fileStringSize << " and eliminated patterns " << stats.GetEradicatedPatterns() << "\n\n\n";
			Logger::WriteLog(crappy.str());

			//File processing validation to make sure every index is processed and eliminated 
			//If we aren't doing a deep search in levels then there isn't a need to check that pattern finder is properly functioning..it's impossible
			if(config.files[f]->fileStringSize != stats.GetEradicatedPatterns() && config.maximum == -1)
			{
				cout << "Houston we are not processing patterns properly!" << endl;
				Logger::WriteLog("Houston we are not processing patterns properly!");
				//exit(0);
			}

			//Clean up memory watch dog thread
			if(memoryQueryThread != NULL)
			{
				processingFinished = true;
				memoryQueryThread->join();
				delete memoryQueryThread;
				memoryQueryThread = NULL;
				processingFinished = false;
			}
			//Clear statistics for next file processing
			stats.ResetData();
		}
		
		//Log throughput data
		stringstream loggingIt;
		loggingIt.str("");
		std::string::size_type i = config.files[f]->fileName.find(DATA_FOLDER);
		string nameage = config.files[f]->fileName;
		if (i != std::string::npos)
			nameage.erase(i, sizeof(DATA_FOLDER)-1);

		loggingIt << "\n" << Logger::GetTime() << " Ended processing for: " << nameage << endl << endl;
		Logger::WriteLog(loggingIt.str());
		cout << loggingIt.str();

		for(pair<uint32_t, double> threadTime : threadMap[f])
		{
			loggingIt.str("");
			loggingIt << "Thread " << threadTime.first << " processed for " << threadTime.second << " milliseconds!\n";
			Logger::WriteLog(loggingIt.str());
			cout << loggingIt.str();
		}
		//Log the percentage of files in folder that have been processed so far
		stringstream fileProgressStream;
		fileProgressStream << "File collection percentage completed: " << f*100/config.files.size() << "%\n";
		Logger::WriteLog(fileProgressStream.str());

		//Close file handle once and for all
		config.files[f]->copyBuffer->clear();
		config.files[f]->fileString.clear();
		config.files[f]->fileString.reserve(0);

		delete config.files[f];

		if(config.findBestThreadNumber)
		{
			config.numThreads = 1;
		}

	}

	//Generate data for file collection processing
	Logger::generateTimeVsFileSizeCSV(processingTimes, config.fileSizes);
	//Generate most patterns found in a large data set
	Logger::generateFinalPatternVsCount(finalPattern);
	//Generate thread throughput data
	if(config.findBestThreadNumber)
	{
		Logger::generateThreadsVsThroughput(threadMap);
	}

	for(int i = 0; i < fileChunks.size(); i++)
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
	crappy.str("");
	crappy << "Errant memory after processing level " << threadMemoryConsumptionInMB - MemoryUsageAtInception << " in MB!\n";
	Logger::WriteLog(crappy.str());

	stringstream mem;
	mem << "Most memory overflow was : " << mostMemoryOverflow << " MB\n";
	Logger::WriteLog(mem.str());
	cout << mem.str();

}

Forest::~Forest()
{
}

void Forest::MemoryQuery()
{
	//Query OS with how much memory the program is using and exit if it has reached the memory limit
	StopWatch swTimer;
	stringstream loggingIt;
	swTimer.Start();
	PListType previousEradicatedPatterns = 0;
	while(!processingFinished)
	{
		this_thread::sleep_for(std::chrono::milliseconds(1));
		double memoryOverflow = 0;
		overMemoryCount = MemoryUtils::IsOverMemoryCount(MemoryUsedPriorToThread, (double)config.memoryBandwidthMB, memoryOverflow);
		currMemoryOverflow = memoryOverflow;
		if(mostMemoryOverflow < memoryOverflow)
		{
			mostMemoryOverflow = memoryOverflow;
		}
		//Abort mission and do not exit gracefully ie dump cause we will be pageing soon
		if(currMemoryOverflow + config.memoryBandwidthMB > memoryCeiling && !config.usingPureRAM)
		{
			Logger::WriteLog("Have to bail because you are using too much memory for your system!");
			exit(0);
		}

		//Every 10 seconds dispaly the percentage of the file processed, cpu utilization of the program and the approximate time left to finish processing
		if(swTimer.GetTime() > 10000.0f)
		{
			PListType timeStamp = static_cast<PListType>(swTimer.GetTime());
			loggingIt.str("");
			swTimer.Start();
			loggingIt << "Thread level status...\n";
			for(PListType j = 0; j < stats.GetCurrentLevelSize(); j++)
			{
				loggingIt << "Thread " << j << " is at level: " << stats.GetCurrentLevel(j) << endl;
			}
			cout << loggingIt.str();
			Logger::WriteLog(loggingIt.str());
			loggingIt.str("");
			loggingIt << "Percentage of file processed is: " << (((double)stats.GetEradicatedPatterns())/((double)config.files[f]->fileStringSize))*100.0f << "%\n";
			cout << loggingIt.str();
			Logger::WriteLog(loggingIt.str());
			loggingIt.str("");
			loggingIt << "Percentage of cpu usage: " << MemoryUtils::CPULoad() << "%\n";
			cout << loggingIt.str();
			Logger::WriteLog(loggingIt.str());
			loggingIt.str("");
			loggingIt << "Approximate processing time left: " << ((((double)(config.files[f]->fileStringSize - stats.GetEradicatedPatterns()))  * timeStamp) / ((double)(stats.GetEradicatedPatterns() - previousEradicatedPatterns)))/1000.0f << " seconds\n";
			cout << loggingIt.str();
			Logger::WriteLog(loggingIt.str());
			initTime.DisplayNow();

			previousEradicatedPatterns = stats.GetEradicatedPatterns();
		}
	}
}


bool Forest::PredictHardDiskOrRAMProcessing(LevelPackage levelInfo, PListType sizeOfPrevPatternCount, PListType sizeOfString)
{
	//Break early if memory usage is predetermined by command line arguments
	if(config.usingPureRAM)
	{
		return false;
	}
	if(config.usingPureHD)
	{
		return true;
	}
	
	//POTENTIAL PATTERNS equals the previous list times 256 possible byte values but this value can't exceed the file size minus the current level
	PListType potentialPatterns = sizeOfPrevPatternCount*256;

	if(potentialPatterns > config.files[f]->fileStringSize - stats.GetEradicatedPatterns())
	{
		//Factor in eradicated patterns because those places no longer need to be checked in the file
		potentialPatterns = config.files[f]->fileStringSize - stats.GetEradicatedPatterns();
	}

	PListType linearListPListLengthsContainerSizesForPrevAndNext = (sizeof(PListType)*(sizeOfString)*2) + (potentialPatterns*sizeof(PListType)*2);  //Predication for containers just predict they will be the same size thus * 2
	
	PListType sizeOfProcessedFile = 0;
	if(levelInfo.currLevel <= 2)
	{
		sizeOfProcessedFile = config.files[f]->fileStringSize;
	}
	else
	{
		sizeOfProcessedFile = config.files[f]->fileStringSize/config.numThreads;
	}

	PListType sizeOfGlobalStringConstruct = sizeOfString;
	PListType totalStorageNeeded = (linearListPListLengthsContainerSizesForPrevAndNext + sizeOfProcessedFile + sizeOfGlobalStringConstruct)/1000000;

	PListType previousLevelMemoryMB = 0;

	double prevMemoryMB = MemoryUtils::GetProgramMemoryConsumption() - MemoryUsageAtInception;
	if(prevMemoryMB > 0.0f)
	{
		previousLevelMemoryMB = (PListType)prevMemoryMB/config.numThreads;
	}

	PListType memoryAllowance = 0;
	if(levelInfo.currLevel <= 2)
	{
		memoryAllowance = config.memoryBandwidthMB;
	}
	else
	{
		memoryAllowance = config.memoryBandwidthMB/config.numThreads;
	}

	if(totalStorageNeeded > memoryAllowance)
	{

		/*stringstream stringbuilder;
		stringbuilder << "Using HARD DISK! Total size for level " << levelInfo.currLevel << " processing is " << totalStorageNeeded << " MB" << endl;
		cout << stringbuilder.str();
		Logger::WriteLog(stringbuilder.str());*/
		return true;
	}
	else
	{
		if(config.files[f]->fileString.size() == 0)
		{
			countMutex->lock();
			config.files[f]->fileString.resize(config.files[f]->fileStringSize);
			config.files[f]->copyBuffer->read( &config.files[f]->fileString[0], config.files[f]->fileString.size());
			countMutex->unlock();
		}

		/*stringstream stringbuilder;
		stringbuilder << "Using DRAM! Total size for level " << levelInfo.currLevel << " processing is " << totalStorageNeeded << " MB" << endl;
		cout << stringbuilder.str();
		Logger::WriteLog(stringbuilder.str());*/

		return false;
	}
}

void Forest::PrepDataFirstLevel(bool prediction, vector<vector<string>>& fileList, vector<vector<PListType>*>* prevLocalPListArray)
{
	vector<vector<string>> tempFileList = fileList;
	for(int a = 0; a < fileList.size(); a++)
	{
		fileList[a].clear();
	}

	//If we need to use the hard disk for processing the upcoming level
	if(prediction)
	{
		//If the previous level was processed using the hard disk then just distribute the pattern files across the threads
		if(!stats.GetUsingRAM(0))
		{
			//chunk files
			vector<PListArchive*> threadFiles;
			stringstream threadFilesNames;
			unsigned int threadNumber = 0;

			for(int a = 0; a < tempFileList.size(); a++)
			{
				for(int b = 0; b < tempFileList[a].size(); b++)
				{
					fileList[threadNumber].push_back(tempFileList[a][b]);

					//Increment chunk
					threadNumber++;
					if(threadNumber >= config.numThreads)
					{
						threadNumber = 0;
					}
				}
			}
		}
		//If previous level was processing using only ram then write pattern vector data to the file system
		else if(stats.GetUsingRAM(0))
		{
			//chunk files
			vector<PListArchive*> threadFiles;
			stringstream threadFilesNames;
			unsigned int threadNumber = 0;

			for(unsigned int a = 0; a < config.numThreads; a++)
			{
				threadFilesNames.str("");
				threadFilesNames << "PListChunks" << chunkFactorio->GenerateUniqueID();

				threadFiles.push_back(new PListArchive(threadFilesNames.str(), true));
				fileList[a].push_back(threadFilesNames.str());
			}
			for(PListType prevIndex = 0; prevIndex < prevLocalPListArray->size(); )
			{
				list<PListType> *sorting = new list<PListType>();

				for(unsigned int threadCount = 0; threadCount < config.numThreads; threadCount++)
				{
					if((*prevLocalPListArray)[prevIndex] != NULL)
					{
						copy( (*prevLocalPListArray)[prevIndex]->begin(), (*prevLocalPListArray)[prevIndex]->end(), std::back_inserter(*sorting));
						((*prevLocalPListArray)[prevIndex])->erase(((*prevLocalPListArray)[prevIndex])->begin(), ((*prevLocalPListArray)[prevIndex])->end());
						delete (*prevLocalPListArray)[prevIndex];
						prevIndex++;
					}
				}
				vector<PListType> finalVector;
				sorting->sort();
				std::copy( sorting->begin(), sorting->end(), std::back_inserter(finalVector));
				sorting->clear();
				delete sorting;

				threadFiles[threadNumber]->WriteArchiveMapMMAP(finalVector);
				threadFiles[threadNumber]->WriteArchiveMapMMAP(vector<PListType>(), "", true);
				

				//Increment chunk
				threadNumber++;
				if(threadNumber >= config.numThreads)
				{
					threadNumber = 0;
				}
			}
			//Clear out the array also after deletion
			prevLocalPListArray->clear();

			for(unsigned int a = 0; a < config.numThreads; a++)
			{
				threadFiles[a]->WriteArchiveMapMMAP(vector<PListType>(), "", true);
				threadFiles[a]->CloseArchiveMMAP();
				delete threadFiles[a];
			}
		}
	}
	//If the upcoming level is going to be processed using ram
	else if(!prediction)
	{
		//If the previous level using hard disk to process then read in pattern file data and convert to pattern vectors
		if(!stats.GetUsingRAM(0))
		{
			prevLocalPListArray->clear();
			for(PListType i = 0; i < tempFileList.size(); i++)
			{
				for(PListType prevChunkCount = 0; prevChunkCount < tempFileList[i].size(); prevChunkCount++)
				{
					PListArchive archive(tempFileList[i][prevChunkCount]);
					while(archive.Exists() && !archive.IsEndOfFile())
					{
						//Just use 100 GB to say we want the whole file for now
						vector<vector<PListType>*> packedPListArray;
						archive.GetPListArchiveMMAP(packedPListArray);

						prevLocalPListArray->insert(prevLocalPListArray->end(), packedPListArray.begin(), packedPListArray.end());

						packedPListArray.erase(packedPListArray.begin(), packedPListArray.end());
					}
					archive.CloseArchiveMMAP();
				}
			}

			for(PListType i = 0; i < config.numThreads; i++)
			{
				if(!config.history)
				{
					chunkFactorio->DeletePatternFiles(tempFileList[i], ARCHIVE_FOLDER);
				}
			}
			//Transition to using entire file when first level was hard disk processing and next level is pure ram
			if(config.files[f]->fileString.size() != config.files[f]->fileStringSize)
			{
				//new way to read in file
				countMutex->lock();
				config.files[f]->copyBuffer->seekg( 0 );
				config.files[f]->fileString.resize(config.files[f]->fileStringSize);
				config.files[f]->copyBuffer->read( &config.files[f]->fileString[0], config.files[f]->fileString.size());
				countMutex->unlock();
			}
		}
	}

	//Set all the threads upcoming level processing mode
	for(unsigned int a = 0; a < config.numThreads; a++)
	{
		stats.SetUsingRAM(0, !prediction);
	}
}

void Forest::PrepData(bool prediction, LevelPackage& levelInfo, vector<string>& fileList, vector<vector<PListType>*>* prevLocalPListArray)
{
	//If the upcoming level is going to be processed using the hard disk
	if(prediction)
	{
		//If the previous level was processed using the hard disk then just distribute the pattern files across the threads
		if(levelInfo.useRAM)
		{
			//chunk file
			PListArchive* threadFile;
			stringstream threadFilesNames;

			fileList.clear();

			threadFilesNames.str("");
			threadFilesNames << "PListChunks" << chunkFactorio->GenerateUniqueID();

			threadFile = new PListArchive(threadFilesNames.str(), true);
			fileList.push_back(threadFilesNames.str());

			for(PListType i = 0; i < prevLocalPListArray->size(); i++)
			{
				list<PListType> *sorting = new list<PListType>();
				copy( (*prevLocalPListArray)[i]->begin(), (*prevLocalPListArray)[i]->end(), std::back_inserter(*sorting));
				((*prevLocalPListArray)[i])->erase(((*prevLocalPListArray)[i])->begin(), ((*prevLocalPListArray)[i])->end());
				sorting->sort();
				std::copy( sorting->begin(), sorting->end(), std::back_inserter(*((*prevLocalPListArray)[i])));
				sorting->clear();
				delete sorting;

				threadFile->WriteArchiveMapMMAP(*(*prevLocalPListArray)[i]);

				delete (*prevLocalPListArray)[i];

				if(threadFile->totalWritten >= PListArchive::writeSize) 
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
	else if(!prediction)
	{
		//If the previous level using hard disk to process then read in pattern file data and convert to pattern vectors
		if(!levelInfo.useRAM)
		{
			for(PListType prevChunkCount = 0; prevChunkCount < fileList.size(); prevChunkCount++)
			{
				PListArchive archive(fileList[prevChunkCount]);
				while(archive.Exists() && !archive.IsEndOfFile())
				{
					//Just use 100 GB to say we want the whole file for now
					vector<vector<PListType>*> packedPListArray;
					archive.GetPListArchiveMMAP(packedPListArray);
					prevLocalPListArray->insert(prevLocalPListArray->end(), packedPListArray.begin(), packedPListArray.end());

					packedPListArray.erase(packedPListArray.begin(), packedPListArray.end());
				}
				archive.CloseArchiveMMAP();
			}
			if(!config.history)
			{
				chunkFactorio->DeletePatternFiles(fileList, ARCHIVE_FOLDER);
			}
			fileList.clear();
		}
	}

	levelInfo.useRAM = !prediction;
	stats.SetUsingRAM(levelInfo.threadIndex, !prediction);

	//Read in all file data back in if the level is using only ram
	if(levelInfo.useRAM && config.files[f]->fileString.size() != config.files[f]->fileStringSize)
	{
		//new way to read in file
		countMutex->lock();
		config.files[f]->copyBuffer->seekg( 0 );
		config.files[f]->fileString.resize(config.files[f]->fileStringSize);
		config.files[f]->copyBuffer->read( &config.files[f]->fileString[0], config.files[f]->fileString.size());
		countMutex->unlock();
	}
}

bool Forest::NextLevelTreeSearch(unsigned int level)
{

	LevelPackage levelInfo;
	levelInfo.currLevel = level;
	levelInfo.inceptionLevelLOL = 0;
	levelInfo.threadIndex = 0;
	levelInfo.useRAM = stats.GetUsingRAM(0);
	levelInfo.coreIndex = 0;

	//Do one prediction for them all
	bool prediction = PredictHardDiskOrRAMProcessing(levelInfo, stats.GetLevelRecording(1), config.files[f]->fileStringSize);

	vector<vector<string>> fileList = prevFileNameList;
	PrepDataFirstLevel(prediction, fileList, prevPListArray);

	//use that one prediction
	if(stats.GetUsingRAM(0))
	{
		//Balance patterns across threads
		vector<vector<PListType>> balancedTruncList = ProcessThreadsWorkLoadRAMFirstLevel(config.numThreads, prevPListArray);
		vector<unsigned int> localWorkingThreads;
		for(unsigned int i = 0; i < balancedTruncList.size(); i++)
		{
			activeThreads[i] = true;
			localWorkingThreads.push_back(i);
		}

		countMutex->lock();
		for (unsigned int i = 0; i < localWorkingThreads.size(); i++)
		{
			//Spawn each ram processing thread with level state information
			LevelPackage levelInfo;
			levelInfo.currLevel = level;
			levelInfo.threadIndex = i;
			levelInfo.inceptionLevelLOL = 0;
			levelInfo.useRAM = true;
			levelInfo.coreIndex = i;
			threadsDispatched++;
			vector<string> temp2;

			threadPool->push_back(std::async(std::launch::async, &Forest::ThreadedLevelTreeSearchRecursionList, this, prevPListArray, balancedTruncList[i], temp2, levelInfo));
		}
		countMutex->unlock();
		WaitForThreads(localWorkingThreads, threadPool);
	}
	//HD processing
	else
	{
		//get ready for distribution
		vector<string> files;
		for(int i = 0; i < fileList.size(); i++)
		{
			for(int j = 0; j < fileList[i].size(); j++)
			{
				files.push_back(fileList[i][j]);
			}
		}
		//Balance pattern files across threads
		vector<vector<string>> balancedTruncList = ProcessThreadsWorkLoadHD(config.numThreads, levelInfo, files);
		vector<unsigned int> localWorkingThreads;
		for(unsigned int i = 0; i < balancedTruncList.size(); i++)
		{
			activeThreads[i] = true;
			localWorkingThreads.push_back(i);
		}
		countMutex->lock();
		for (unsigned int i = 0; i < localWorkingThreads.size(); i++)
		{
			//Spawn each hd processing thread with level state information
			LevelPackage levelInfo;
			levelInfo.currLevel = level;
			levelInfo.threadIndex = i;
			levelInfo.inceptionLevelLOL = 0;
			levelInfo.useRAM = false;
			levelInfo.coreIndex = i;
			threadsDispatched++;
			vector<PListType> temp;
			threadPool->push_back(std::async(std::launch::async, &Forest::ThreadedLevelTreeSearchRecursionList, this, prevPListArray, temp, balancedTruncList[i], levelInfo));
		}
		countMutex->unlock();
		WaitForThreads(localWorkingThreads, threadPool);
	}
	
	prevPListArray->clear();

	threadPool->erase(threadPool->begin(), threadPool->end());
	(*threadPool).clear();

	return false;
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

			if (localThreadPool != NULL && (*localThreadPool)[localWorkingThreads[k]].valid())
			{
				if(recursive)
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

					if(level != 0)
					{
						stringstream buff;
						buff << "Thread " << localWorkingThreads[k] << " finished all processing" << endl;
						Logger::WriteLog(buff.str());
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
		for(unsigned int i = 0; i < currentThreads.size(); i++)
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
	if(prevFileNames.size() >= threadsToDispatch)
	{

		//Not distributing the pattern filess
		for(int a = 0; a < prevFileNames.size(); a++)
		{
			newFileList[threadNumber].push_back(prevFileNames[a]);

			//Increment chunk
			threadNumber++;
			if(threadNumber >= threadsToDispatch)
			{
				threadNumber = 0;
			}
		}
	}
	else
	{

		//Distributing files
		for(unsigned int a = 0; a < threadsToDispatch; a++)
		{
			threadFilesNames.str("");
			threadFilesNames << "PListChunks" << chunkFactorio->GenerateUniqueID();

			threadFiles.push_back(new PListArchive(threadFilesNames.str(), true));
			newFileList[a].push_back(threadFilesNames.str());

		}

		//Read in all pattern data from file and create new pattern files to evenly distribute pattern data to the threads
		for(PListType prevChunkCount = 0; prevChunkCount < prevFileNames.size(); prevChunkCount++)
		{
			PListArchive archive(prevFileNames[prevChunkCount]);

			while(archive.Exists() && !archive.IsEndOfFile())
			{
				//Just use 100 GB to say we want the whole file for now
				vector<vector<PListType>*> packedPListArray;
				archive.GetPListArchiveMMAP(packedPListArray);

				for(PListType prevIndex = 0; prevIndex < packedPListArray.size(); prevIndex++)
				{
					threadFiles[threadNumber]->WriteArchiveMapMMAP(*(packedPListArray[prevIndex]));
					delete packedPListArray[prevIndex];

					//Increment chunk
					threadNumber++;
					if(threadNumber >= threadsToDispatch)
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
		for(unsigned int a = 0; a < threadsToDispatch; a++)
		{
			threadFiles[a]->WriteArchiveMapMMAP(vector<PListType>(), "", true);
			threadFiles[a]->CloseArchiveMMAP();
			delete threadFiles[a];
		}

	}

	return newFileList;
}

vector<vector<PListType>> Forest::ProcessThreadsWorkLoadRAM(unsigned int threadsToDispatch, vector<vector<PListType>*>* patterns)
{
	//Evenly distribute pattern data by breaking down the pattern vector and adding them to the thread processing pattern vectors
	vector<vector<PListType>> balancedList(threadsToDispatch);
	vector<PListType> balancedSizeList;
	for(PListType i = 0; i < threadsToDispatch; i++)
	{
		balancedSizeList.push_back(0);
	}
	for(PListType i = 0; i < patterns->size(); i++)
	{

		if((*patterns)[i] != NULL)
		{
			bool found = false;
			PListType smallestIndex = 0;
			PListType smallestAmount = -1;
			for(PListType z = 0; z < threadsToDispatch; z++)
			{
				if(balancedSizeList[z] < smallestAmount && (*patterns)[i]->size() > 0)
				{
					smallestAmount = balancedSizeList[z];
					smallestIndex = z;
					found = true;
				}
			}
			if(found && (*patterns)[i]->size() > 0)
			{
				balancedSizeList[smallestIndex] += static_cast<PListType>((*patterns)[i]->size());
				balancedList[smallestIndex].push_back(i);
			}
		}
	}
	PListType sizeToTruncate = 0;
	for(unsigned int i = 0; i < threadsToDispatch; i++)
	{
		if(balancedList[i].size() > 0)
		{
			sizeToTruncate++;
		}
	}

	vector<vector<PListType>> balancedTruncList(sizeToTruncate);
	PListType internalCount = 0;
	for(unsigned int i = 0; i < threadsToDispatch; i++)
	{
		if(balancedList[i].size() > 0)
		{
			balancedTruncList[internalCount] = balancedList[i];
			internalCount++;
		}
	}

	vector<unsigned int> localWorkingThreads;
	for(unsigned int i = 0; i < sizeToTruncate; i++)
	{
		localWorkingThreads.push_back(i);
	}
	
	return balancedTruncList;
}

vector<vector<PListType>> Forest::ProcessThreadsWorkLoadRAMFirstLevel(unsigned int threadsToDispatch, vector<vector<PListType>*>* patterns)
{
	//Evenly distribute pattern data by breaking down the pattern vector and adding them to the thread processing pattern vectors for the first level
	vector<vector<PListType>> balancedList(threadsToDispatch);
	vector<PListType> balancedSizeList;
	for(PListType i = 0; i < threadsToDispatch; i++)
	{
		balancedSizeList.push_back(0);
	}

	if(patterns->size() == threadsToDispatch*256)
	{
		vector<PListType> patternTotals(256, 0);
		for(PListType i = 0; i < 256; i++)
		{
			for(PListType z = 0; z < threadsToDispatch; z++)
			{
				patternTotals[i] += static_cast<PListType>((*patterns)[i*threadsToDispatch + z]->size());
			}
		}

		for(PListType i = 0; i < patternTotals.size(); i++)
		{
			bool found = false;
			PListType smallestIndex = 0;
			PListType smallestAmount = -1;
			for(PListType j = 0; j < threadsToDispatch; j++)
			{
				if((*patterns)[(i*threadsToDispatch) + j] != NULL)
				{
					for(PListType z = 0; z < threadsToDispatch; z++)
					{
						if(balancedSizeList[z] < smallestAmount)
						{
							smallestAmount = balancedSizeList[z];
							smallestIndex = z;
							found = true;
						}
					}

				}
			}
			if(found)
			{
				balancedSizeList[smallestIndex] += patternTotals[i];
				for(PListType j = 0; j < threadsToDispatch; j++)
				{
					balancedList[smallestIndex].push_back((i*threadsToDispatch) + j);
				}
			}
		}
	}
	else
	{
		vector<PListType> patternTotals;
		for(PListType i = 0; i < patterns->size(); i++)
		{
			patternTotals.push_back(static_cast<PListType>((*patterns)[i]->size()));
		}

		for(PListType i = 0; i < patternTotals.size(); i++)
		{
			bool found = false;
			PListType smallestIndex = 0;
			PListType smallestAmount = -1;
			
			if((*patterns)[i] != NULL)
			{
				for(PListType z = 0; z < threadsToDispatch; z++)
				{
					if(balancedSizeList[z] < smallestAmount)
					{
						smallestAmount = balancedSizeList[z];
						smallestIndex = z;
						found = true;
					}
				}
			}
			if(found)
			{
				balancedSizeList[smallestIndex] += patternTotals[i];
				balancedList[smallestIndex].push_back(i);
			}
		}
	}

	PListType sizeToTruncate = 0;
	for(unsigned int i = 0; i < threadsToDispatch; i++)
	{
		if(balancedList[i].size() > 0)
		{
			sizeToTruncate++;
		}
	}

	vector<vector<PListType>> balancedTruncList(sizeToTruncate);
	PListType internalCount = 0;
	for(unsigned int i = 0; i < threadsToDispatch; i++)
	{
		if(balancedList[i].size() > 0)
		{
			balancedTruncList[internalCount] = balancedList[i];
			internalCount++;
		}
	}

	return balancedTruncList;
}

PListType Forest::ProcessChunksAndGenerate(vector<string> fileNamesToReOpen, vector<string>& newFileNames, PListType memDivisor, unsigned int threadNum, unsigned int currLevel, unsigned int coreIndex, bool firstLevel)
{

	int currentFile = 0;
	int prevCurrentFile = currentFile;
	bool memoryOverflow = false;
	PListType interimCount = 0;
	unsigned int threadNumber = 0;

	//Grab all the partial pattern files to bring together into one coherent pattern file structure
	vector<string> fileNamesBackup;
	for(int a = 0; a < fileNamesToReOpen.size(); a++)
	{
		fileNamesBackup.push_back(fileNamesToReOpen[a]);
	}

	vector<PListArchive*> patternFiles;
	PListType internalRemovedCount = 0;

	while(currentFile < fileNamesBackup.size())
	{
		memoryOverflow = false;

		vector<PListArchive*> archiveCollection;
		map<PatternType, vector<PListType>*> finalMetaDataMap;

		for(int a = 0; a < fileNamesBackup.size() - prevCurrentFile; a++)
		{

			archiveCollection.push_back(new PListArchive(fileNamesBackup[a+prevCurrentFile]));

			//If file size is 0 or does not exist, we skip
			if(archiveCollection[a]->IsEndOfFile())
			{
				if(!memoryOverflow)
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
			vector<string> *stringBuffer = NULL;
			PListArchive *stringBufferFile = NULL;
			string fileNameForLater ="";
			PListType packedPListSize = 0; 
			string fileName ="";
			bool foundAHit = true;

			if(overMemoryCount && finalMetaDataMap.size() > 0)
			{
				memoryOverflow = true;
			}

			//If memory has not been exceeded then continue compiling pattern map data
			if(!memoryOverflow)
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
				PListType sizeOfPackedPList = static_cast<PListType>(std::stoll (copyString,&sz));
				stringBuffer = stringBufferFile->GetPatterns(currLevel, packedPListSize);

			}
			else
			{
				//If memory has been overused in the final pattern map then we see which patterns can be stored in file and which need to be kept around
				list<string> patternsThatCantBeDumped;
				PListType totalPatterns = 0;

				for(int earlyWriteIndex = a; earlyWriteIndex < fileNamesBackup.size() - prevCurrentFile; earlyWriteIndex++)
				{

					string tempString = fileNamesBackup[earlyWriteIndex + prevCurrentFile];

					tempString.append("Patterns");
					PListArchive *stringBufferFileLocal = new PListArchive(tempString);

					std::string::size_type j = fileNamesBackup[earlyWriteIndex + prevCurrentFile].find("_");
					string copyString = fileNamesBackup[earlyWriteIndex + prevCurrentFile];
					copyString.erase(0, j + 1);
					std::string::size_type sz;   // alias of size_t
					PListType sizeOfPackedPList = static_cast<PListType>(std::stoll (copyString,&sz));
					vector<string> *stringBufferLocal = stringBufferFileLocal->GetPatterns(currLevel, sizeOfPackedPList);

					stringBufferFileLocal->CloseArchiveMMAP();
					delete stringBufferFileLocal;

					totalPatterns += sizeOfPackedPList;

					if(sizeOfPackedPList > 0 && stringBufferLocal != NULL)
					{
						for(PListType z = 0; z < stringBufferLocal->size(); z++)
						{
							if(finalMetaDataMap.find((*stringBufferLocal)[z]) != finalMetaDataMap.end())
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
				if(patternsThatCantBeDumped.size() == 0)
				{ 
					//thread files
					PListArchive* currChunkFile = NULL;
					bool notBegun = true;

					auto iterator = finalMetaDataMap.begin();
					while( iterator != finalMetaDataMap.end())
					{

						if(notBegun)
						{
							notBegun = false;
							if(currChunkFile != NULL)
							{
								currChunkFile->WriteArchiveMapMMAP(vector<PListType>(), "", true);
								currChunkFile->CloseArchiveMMAP();
								delete currChunkFile;
							}

							if(firstLevel)
							{
								stringstream fileNameage;
								stringstream fileNameForPrevList;
								PListType newID = chunkFactorio->GenerateUniqueID();
								fileNameage << ARCHIVE_FOLDER << "PListChunks" << newID << ".txt";
								fileNameForPrevList << "PListChunks" << newID;

								prevFileNameList[threadNumber].push_back(fileNameForPrevList.str());

								currChunkFile = new PListArchive(fileNameForPrevList.str(), true);

								threadNumber++;
								threadNumber %= config.numThreads;
							}
							else
							{

								stringstream fileNameForPrevList;
								fileNameForPrevList << "PListChunks" << chunkFactorio->GenerateUniqueID();

								newFileNames.push_back(fileNameForPrevList.str());

								currChunkFile = new PListArchive(fileNameForPrevList.str(), true);
							}
						}

						if(iterator->second->size() >= config.minOccurrence)
						{
							currChunkFile->WriteArchiveMapMMAP(*iterator->second);
							interimCount++;
							stats.SetMostCommonPattern(currLevel, static_cast<PListType>(iterator->second->size()), (*iterator->second)[0] - currLevel);

							//Record level statistics
							countMutex->lock();
							stats.SetTotalOccurrenceFrequency(currLevel, iterator->second->size());

							//If levelToOutput is not selected but -Pall is set or if -Pall is set and -Plevel is set to output data only for a specific level
							if(config.levelToOutput == 0 || config.levelToOutput == currLevel)
							{
								PListType distances = 0;
								PListType index = 0;
								PListType length = iterator->second->size();
								PListType coverageSubtraction = 0;
								//Calculate average distance between pattern instances
								for(int i = index; i < index + length - 1; i++)
								{
									distances += (*iterator->second)[i+1] - (*iterator->second)[i];
									if((*iterator->second)[i+1] - (*iterator->second)[i] < currLevel)
									{
										coverageSubtraction += currLevel - ((*iterator->second)[i+1] - (*iterator->second)[i]);
									}
								}

								float averageDistance = ((float)distances)/((float)(length - 1));
								stringstream data;

								//Struct used to contain detailed pattern information for one level
								ProcessorStats::DisplayStruct outputData;
								outputData.patternInstances = length;
								outputData.patternCoveragePercentage = (float)100.0f*(((length*currLevel) - coverageSubtraction))/(float)config.files[f]->fileStringSize;
								outputData.averagePatternDistance = averageDistance;
								outputData.firstIndexToPattern = (*iterator->second)[index];
				
								//If pnoname is not selected then strings are written to log, this could be for reasons where patterns are very long
								if(!config.suppressStringOutput)
								{
									outputData.pattern = config.files[f]->fileString.substr((*iterator->second)[index] - currLevel, currLevel);
								}
								stats.detailedLevelInfo.push_back(outputData);
							}

							countMutex->unlock();

						}
						else
						{
							internalRemovedCount++;
						}

						delete iterator->second;
						iterator = finalMetaDataMap.erase(iterator);
					}

					if(currChunkFile != NULL)
					{
						currChunkFile->WriteArchiveMapMMAP(vector<PListType>(), "", true);
						if(currChunkFile->mappingIndex != 0)
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
				PListType sizeOfPackedPList = static_cast<PListType>(std::stoll (copyString,&sz));
				stringBufferFile = new PListArchive(fileName);
				stringBuffer = stringBufferFile->GetPatterns(currLevel, sizeOfPackedPList);
				packedPListSize = sizeOfPackedPList;

				//If the remaining files have patterns that are contained in them that are contained in the final map then we need to hold onto those patterns for later
				if(finalMetaDataMap.size() > 0)
				{
					if(sizeOfPackedPList > 0)
					{
						for(PListType z = 0; z < stringBuffer->size(); z++)
						{
							if(finalMetaDataMap.find((*stringBuffer)[z]) != finalMetaDataMap.end())
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

				if(foundAHit)
				{
					archiveCollection[a]->GetPListArchiveMMAP(packedPListArray); //Needs MB
				}

			}

			PListType countAdded = 0;
			//Write all patterns to file except the ones that have a presence in the remaining partial files
			if(foundAHit)
			{
				for(PListType partialLists = 0; partialLists < packedPListArray.size(); partialLists++)
				{
					try
					{
						//This allows us to back fill the others iterations when this didn't have a pattern
						if(finalMetaDataMap.find((*stringBuffer)[partialLists]) == finalMetaDataMap.end())
						{
							if(!memoryOverflow)
							{
								finalMetaDataMap[(*stringBuffer)[partialLists]] = new vector<PListType>(packedPListArray[partialLists]->begin(), packedPListArray[partialLists]->end());
								delete packedPListArray[partialLists];
								packedPListArray[partialLists] = NULL;
								countAdded++;
							}
						}
						else
						{
							finalMetaDataMap[(*stringBuffer)[partialLists]]->insert(finalMetaDataMap[(*stringBuffer)[partialLists]]->end(), packedPListArray[partialLists]->begin(), packedPListArray[partialLists]->end());
							delete packedPListArray[partialLists];
							packedPListArray[partialLists] = NULL;
							countAdded++;
						}

					}
					catch(exception e)
					{
						cout << "System exception: " << e.what() << endl;
					}
				}
			}


			if(foundAHit)
			{
				archiveCollection[a]->CloseArchiveMMAP();
				stringBufferFile->CloseArchiveMMAP();

				chunkFactorio->DeletePartialPatternFile(fileNameForLater, ARCHIVE_FOLDER);

				delete archiveCollection[a];
				delete stringBufferFile;


				PListType newCount = packedPListSize - countAdded;
				if(newCount > 0)
				{
					stringstream namer;
					namer << "PListChunks" << chunkFactorio->GenerateUniqueID() << "_" << newCount;

					fileNamesBackup[prevCurrentFile + a] = namer.str();

					PListType testCount = 0;
					PListArchive* archiveCollective = NULL;
					
					archiveCollective = new PListArchive(fileNamesBackup[prevCurrentFile + a], true);

					for(PListType partialLists = 0; partialLists < packedPListSize; partialLists++)
					{
						if(packedPListArray[partialLists] != NULL)
						{
							archiveCollective->WriteArchiveMapMMAP(*packedPListArray[partialLists], (*stringBuffer)[partialLists]);
							delete packedPListArray[partialLists];
							packedPListArray[partialLists] = NULL;
							testCount++;
						}
					}
					archiveCollective->DumpPatternsToDisk(currLevel);
					archiveCollective->WriteArchiveMapMMAP(vector<PListType>(), "", true);
					archiveCollective->CloseArchiveMMAP();

					delete archiveCollective;
				}
				if(stringBuffer != NULL)
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
			if(!memoryOverflow || countAdded == packedPListSize || packedPListSize == 0)
			{
				currentFile++;
			}
		}

		//Start writing the patterns in pattern map to disk for complete pattern pictures
		PListArchive* currChunkFile = NULL;
		bool notBegun = true;
		
		for(auto iterator = finalMetaDataMap.begin(); iterator != finalMetaDataMap.end(); iterator++)
		{
			if(notBegun)
			{
				notBegun = false;
				if(currChunkFile != NULL)
				{
					currChunkFile->WriteArchiveMapMMAP(vector<PListType>(), "", true);
					currChunkFile->CloseArchiveMMAP();
					delete currChunkFile;
				}

				if(firstLevel)
				{
					stringstream fileNameage;
					stringstream fileNameForPrevList;
					PListType newID = chunkFactorio->GenerateUniqueID();
					fileNameage << ARCHIVE_FOLDER << "PListChunks" << newID << ".txt";
					fileNameForPrevList << "PListChunks" << newID;

					prevFileNameList[threadNumber].push_back(fileNameForPrevList.str());

					currChunkFile = new PListArchive(fileNameForPrevList.str(), true);

					threadNumber++;
					threadNumber %= config.numThreads;
				}
				else
				{

					stringstream fileNameForPrevList;
					fileNameForPrevList << "PListChunks" << chunkFactorio->GenerateUniqueID();

					newFileNames.push_back(fileNameForPrevList.str());

					currChunkFile = new PListArchive(fileNameForPrevList.str(), true);
				}
			}

			if(iterator->second->size() >= config.minOccurrence)
			{
				currChunkFile->WriteArchiveMapMMAP(*iterator->second);
				interimCount++;
				stats.SetMostCommonPattern(currLevel, static_cast<PListType>(iterator->second->size()), (*iterator->second)[0] - currLevel);

				//Record level statistics
				countMutex->lock();
				stats.SetTotalOccurrenceFrequency(currLevel, iterator->second->size());

				//If levelToOutput is not selected but -Pall is set or if -Pall is set and -Plevel is set to output data only for a specific level
				if(config.levelToOutput == 0 || config.levelToOutput == currLevel)
				{
					PListType index = 0;
					PListType length = iterator->second->size();
					PListType coverageSubtraction = 0;
					PListType distances = 0;
					//Calculate average distance between pattern instances
					for(int i = index; i < index + length - 1; i++)
					{
						distances += (*iterator->second)[i+1] - (*iterator->second)[i];
						if((*iterator->second)[i+1] - (*iterator->second)[i] < currLevel)
						{
							coverageSubtraction += currLevel - ((*iterator->second)[i+1] - (*iterator->second)[i]);
						}
					}

					float averageDistance = ((float)distances)/((float)(length - 1));
					stringstream data;

					//Struct used to contain detailed pattern information for one level
					ProcessorStats::DisplayStruct outputData;
					outputData.patternInstances = length;
					outputData.patternCoveragePercentage = (float)100.0f*(((length*currLevel) - coverageSubtraction))/(float)config.files[f]->fileStringSize;
					outputData.averagePatternDistance = averageDistance;
					outputData.firstIndexToPattern = (*iterator->second)[index];
				
					//If pnoname is not selected then strings are written to log, this could be for reasons where patterns are very long
					if(!config.suppressStringOutput)
					{
						outputData.pattern = config.files[f]->fileString.substr((*iterator->second)[index] - currLevel, currLevel);
					}
					stats.detailedLevelInfo.push_back(outputData);
				}

				countMutex->unlock();

			}
			else
			{
				internalRemovedCount++;
			}

			delete iterator->second;
		}

		if(currChunkFile != NULL)
		{
			currChunkFile->WriteArchiveMapMMAP(vector<PListType>(), "", true);
			if(currChunkFile->mappingIndex != 0)
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
		for(int a = prevCurrentFile; a < currentFile; a++)
		{
			chunkFactorio->DeletePartialPatternFile(fileNamesBackup[a], ARCHIVE_FOLDER);
		}
		prevCurrentFile = currentFile;
	}

	//Record level statistics
	countMutex->lock();

	stats.SetEradicationsPerLevel(currLevel, stats.GetEradicationsPerLevel(currLevel) + internalRemovedCount);
	stats.SetEradicatedPatterns(stats.GetEradicatedPatterns() + internalRemovedCount);

	stats.SetLevelRecording(currLevel, stats.GetLevelRecording(currLevel) + interimCount);
	
	stats.SetCurrentLevel(threadNum, currLevel + 1);
	
	countMutex->unlock();

	return interimCount;
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
	if(currLevel == 1)
	{
		currPatternCount = 256;
	}
	else
	{
		currPatternCount = 256*stats.GetLevelRecording(currLevel);
	}

	map<string, PListArchive*> currChunkFiles;
	for(PListType a = 0; a < currPatternCount; a++)
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

		threadNumber++;
		threadNumber %= config.numThreads;
	}

	vector<string> fileNamesBackup;
	for(int a = 0; a < fileNamesToReOpen.size(); a++)
	{
		fileNamesBackup.push_back(fileNamesToReOpen[a]);
	}

	map<string, pair<PListType, PListType>> patternCounts;
	while(currentFile < fileNamesBackup.size())
	{
		memoryOverflow = false;

		vector<PListArchive*> archiveCollection;
		
		PListType distances[256] = {0};
		PListType coverageSubtraction[256] = {0};

		for(int a = 0; a < fileNamesBackup.size() - prevCurrentFile; a++)
		{

			archiveCollection.push_back(new PListArchive(fileNamesBackup[a+prevCurrentFile]));

			//Our job is to trust whoever made the previous chunk made it within the acceptable margin of error so we compensate by saying take up to double the size if the previous
			//chunk went a little over the allocation constraint

			vector<vector<PListType>*> packedPListArray;
			vector<string> *stringBuffer = NULL;
			PListArchive *stringBufferFile = NULL;
			string fileNameForLater ="";
			PListType packedPListSize = 0; 
			string fileName ="";
			bool foundAHit = true;

			//Check for memory over usage
			if(overMemoryCount && !memoryOverflow)
			{
				stringstream crap;
				crap << "Overflow at Process Chunks And Generate of " << currMemoryOverflow << " in MB!\n";
				Logger::WriteLog(crap.str());
			}
			else if(overMemoryCount && memoryOverflow)
			{
				stringstream crap;
				crap << "Overflow at Process Chunks And Generate of " << currMemoryOverflow << " in MB!\n";
				Logger::WriteLog(crap.str());
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
			PListType sizeOfPackedPList = static_cast<PListType>(std::stoll (copyString,&sz));
			stringBuffer = stringBufferFile->GetPatterns(currLevel, packedPListSize);



			PListType countAdded = 0;
			//Write all patterns contained in the pattern map to complete pattern files
			if(foundAHit)
			{
				for(PListType partialLists = 0; partialLists < packedPListArray.size(); partialLists++)
				{
					try
					{

						if(overMemoryCount && !memoryOverflow )
						{
							stringstream crap;
							crap << "Overflow at Process Chunks And Generate of " << currMemoryOverflow << " in MB!\n";
							Logger::WriteLog(crap.str());
						}

						string pattern = (*stringBuffer)[partialLists];
						
						if(patternCounts.find(pattern) != patternCounts.end())
						{
							patternCounts[pattern].first += static_cast<PListType>(packedPListArray[partialLists]->size());
						}
						else
						{
							patternCounts[pattern].first = static_cast<PListType>(packedPListArray[partialLists]->size());
							patternCounts[pattern].second = (*(packedPListArray[partialLists]))[0];
						}

						unsigned char val = (unsigned char)pattern[0];
						for(int i = 0; i < packedPListArray[partialLists]->size() - 1; i++)
						{
							distances[val] += (*(packedPListArray[partialLists]))[i+1] - (*(packedPListArray[partialLists]))[i];
							if((*(packedPListArray[partialLists]))[i+1] - (*(packedPListArray[partialLists]))[i] < currLevel)
							{
								coverageSubtraction[val] += currLevel - ((*(packedPListArray[partialLists]))[i+1] - (*(packedPListArray[partialLists]))[i]);
							}
						}

						currChunkFiles[pattern]->WriteArchiveMapMMAP(*(packedPListArray[partialLists]));
						delete packedPListArray[partialLists];
						packedPListArray[partialLists] = NULL;

					}
					catch(exception e)
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


			if(stringBuffer != NULL)
			{
				stringBuffer->clear();
				delete stringBuffer;
			}

			delete stringBufferFile;

			if(!memoryOverflow)
			{
				currentFile++;
			}
		}

		//Record level 1 statistics
		for(PListType a = 0; a < currPatternCount; a++)
		{
			stringstream buff;
			buff << (char)a;
			currChunkFiles[buff.str()]->WriteArchiveMapMMAP(vector<PListType>(), "", true);
			bool empty = true;
			PListType patterCount = (currChunkFiles[buff.str()]->prevMappingIndex/sizeof(PListType)) - sizeof(PListType);

			//Keeps track of the index in pListLengths vector
			vector<PListType> positionsInLinearList;

			if(currChunkFiles[buff.str()]->mappingIndex > (2*sizeof(PListType)))
			{
				empty = false;
				interimCount++;
				stats.SetMostCommonPattern(currLevel, patternCounts[buff.str()].first, patternCounts[buff.str()].second - currLevel);

				stats.SetTotalOccurrenceFrequency(currLevel, patternCounts[buff.str()].first);

				//If levelToOutput is not selected but -Pall is set or if -Pall is set and -Plevel is set to output data only for a specific level
				if(config.levelToOutput == 0 || config.levelToOutput == currLevel)
				{
					PListType length = patternCounts[buff.str()].first;
					
					float averageDistance = ((float)distances[a])/((float)(length - 1));
					stringstream data;

					//Struct used to contain detailed pattern information for one level
					ProcessorStats::DisplayStruct outputData;
					outputData.patternInstances = length;
					outputData.patternCoveragePercentage = (float)100.0f*(((length*currLevel) - coverageSubtraction[a]))/(float)config.files[f]->fileStringSize;
					outputData.averagePatternDistance = averageDistance;
					outputData.firstIndexToPattern = patternCounts[buff.str()].second - currLevel + 1;
				
					//If pnoname is not selected then strings are written to log, this could be for reasons where patterns are very long
					if(!config.suppressStringOutput)
					{
						outputData.pattern = config.files[f]->fileString.substr(patternCounts[buff.str()].second - currLevel, currLevel);
					}
					stats.detailedLevelInfo.push_back(outputData);
				}

			}
			else if(currChunkFiles[buff.str()]->mappingIndex == (2*sizeof(PListType)))
			{
				removedPatterns++;
			}

			string fileToDelete = currChunkFiles[buff.str()]->patternName;
			currChunkFiles[buff.str()]->CloseArchiveMMAP();
			delete currChunkFiles[buff.str()];

			if(fileNamesBackup.size() == currentFile && empty)
			{
				chunkFactorio->DeletePartialPatternFile(fileToDelete, ARCHIVE_FOLDER);
			}
		}
	}

	countMutex->lock();

	stats.SetEradicationsPerLevel(currLevel, stats.GetEradicationsPerLevel(currLevel) + removedPatterns);
	stats.SetEradicatedPatterns(stats.GetEradicatedPatterns() + removedPatterns);

	stats.SetLevelRecording(currLevel, stats.GetLevelRecording(currLevel) + interimCount);
	stats.SetCoverage(currLevel, ((float)(interimCount))/((float)config.files[f]->fileStringSize));
	stats.SetCurrentLevel(threadNum, currLevel + 1);
	
	countMutex->unlock();

	return interimCount;
}
PListType Forest::ProcessHD(LevelPackage& levelInfo, vector<string>& fileList, bool &isThreadDefuncted)
{
	double threadMemoryConsumptionInMB = MemoryUtils::GetProgramMemoryConsumption();

	int threadNum = levelInfo.threadIndex;
	
	PListType newPatternCount = 0;
	//Divide between file load and previous level pLists and leave some memory for new lists 
	PListType memDivisor = (PListType)(((memoryPerThread*1000000)/3.0f));

	bool morePatternsToFind = true;

	unsigned int fileIters = (unsigned int)(config.files[f]->fileStringSize/memDivisor);
	if(config.files[f]->fileStringSize%memDivisor != 0)
	{
		fileIters++;
	}

	try
	{
		while(morePatternsToFind)
		{
			
			PListType removedPatterns = 0;
			newPatternCount = 0;
			vector<string> fileNamesToReOpen;
			string saveOffPreviousStringData = "";
			vector<string> newFileNames;

			unsigned int currLevel = levelInfo.currLevel;
			//Use previous pattern files to process new files
			for(PListType prevChunkCount = 0; prevChunkCount < fileList.size(); prevChunkCount++)
			{
				PListArchive archive(fileList[prevChunkCount]);
				while(!archive.IsEndOfFile())
				{
					//Pull in 1/3 of the memory bandwidth given for this thread
					vector<vector<PListType>*> packedPListArray;
					archive.GetPListArchiveMMAP(packedPListArray, memDivisor/1000000.0f); 

					if(packedPListArray.size() > 0)
					{
						PListType packedListSize = static_cast<PListType>(packedPListArray.size());
						vector <PListType> prevLeafIndex(packedListSize, 0);

						//Get minimum and maximum indexes so we can see if some chunks can be skipped from being loaded bam!
						PListType minimum = -1;
						PListType maximum = 0;
						for(int m = 0; m < packedPListArray.size(); m++)
						{
							for(int n = 0; n < packedPListArray[m]->size(); n++)
							{
								if((*packedPListArray[m])[0] < minimum)
								{
									minimum = (*packedPListArray[m])[0];
								}
							}
						}

						unsigned int firstIndex = (unsigned int)(minimum/memDivisor);
						unsigned int lastIndex = fileIters;

						int threadChunkToUse = threadNum;
						//Pull in 1/3 of the memory bandwidth for the actual file data
						for(unsigned int j = firstIndex; j < lastIndex && minimum != -1; j++)
						{
							if(packedListSize > 0)
							{
								if(fileChunks.size() > threadChunkToUse && fileChunks[threadChunkToUse].size() > 0)
								{
									saveOffPreviousStringData = fileChunks[threadChunkToUse].substr(fileChunks[threadChunkToUse].size() - (currLevel - 1), currLevel - 1);
								}

								PListType patternCount = 0;
								if(config.files[f]->fileStringSize <= memDivisor*j + memDivisor)
								{
									patternCount = config.files[f]->fileStringSize - memDivisor*j;
								}
								else
								{
									patternCount = memDivisor;
								}


								countMutex->lock();
								bool foundChunkInUse = false;
								for(auto iterator = chunkIndexToFileChunk.begin(); iterator != chunkIndexToFileChunk.end(); iterator++)
								{
									if(iterator->first == j)
									{
										threadChunkToUse = iterator->second;
										foundChunkInUse = true;
										break;
									}
								}

								if(!foundChunkInUse)
								{
									writingFlag = true;

									//Shared file space test
									fileChunks.push_back("");
									fileChunks[fileChunks.size() - 1].resize(patternCount);

									PListType offset = memDivisor*j;
									bool isFile;
									FileReader fileReaderTemp(config.files[f]->fileName, isFile, true);
									fileReaderTemp.copyBuffer->seekg( offset );
									fileReaderTemp.copyBuffer->read( &fileChunks[fileChunks.size() - 1][0], patternCount );

									threadChunkToUse = (int)(fileChunks.size() - 1);

									chunkIndexToFileChunk[j] = threadChunkToUse;

									writingFlag = false;

								}
								else
								{
									//otherwise use what has already been lifted from file
								}
								countMutex->unlock();

							}

							bool justPassedMemorySize = false;


							vector<TreeHD> leaflet;

							for(PListType i = 0; i < packedListSize; i++)
							{
								vector<PListType>* pList = packedPListArray[i];
								PListType pListLength = static_cast<PListType>(packedPListArray[i]->size());
								PListType k = prevLeafIndex[i];

								string headPattern = "";
								bool grabbedHead = false;

								
								if( !overMemoryCount)
								{
									leaflet.push_back(TreeHD());

									PListSignedType relativeIndex = 0;
									PListType indexForString = 0;
									while( k < pListLength && ((*pList)[k]) < (j+1)*memDivisor )
									{
										if(!writingFlag)
										{

											try
											{
												if(((*pList)[k]) < config.files[f]->fileStringSize)
												{
													//If index comes out to be larger than fileString than it is a negative number 
													//and we must use previous string data!
													if(((((*pList)[k]) - memDivisor*j) - (currLevel-1)) >= config.files[f]->fileStringSize)
													{
														relativeIndex = ((((*pList)[k]) - memDivisor*j) - (currLevel-1));
														string pattern = "";
														relativeIndex *= -1;
														indexForString = static_cast<PListType>(saveOffPreviousStringData.size()) - relativeIndex;
														if(saveOffPreviousStringData.size() > 0 && relativeIndex > 0)
														{
															pattern = saveOffPreviousStringData.substr(indexForString, relativeIndex);
															pattern.append(fileChunks[threadChunkToUse].substr(0, currLevel - pattern.size()));
															leaflet.back().addLeaf((*pList)[k]+1, pattern.back());
															
															if(!grabbedHead)
															{
																grabbedHead = true;
																headPattern = fileChunks[threadChunkToUse].substr(((((*pList)[k]) - memDivisor*j) - (currLevel-1)), currLevel - 1);
																leaflet.back().setHeadLeaf(headPattern);
															}

														}
													}
													else
													{
														//If pattern is past end of string stream then stop counting this pattern
														if(((*pList)[k]) < config.files[f]->fileStringSize)
														{
															leaflet.back().addLeaf((*pList)[k]+1, fileChunks[threadChunkToUse][(((*pList)[k]) - memDivisor*j)]);

															if(!grabbedHead)
															{
																grabbedHead = true;
																headPattern = fileChunks[threadChunkToUse].substr(((((*pList)[k]) - memDivisor*j) - (currLevel-1)), currLevel - 1);
																leaflet.back().setHeadLeaf(headPattern);
															}
														}
														else if(((((*pList)[k]) - memDivisor*j) - (currLevel-1)) < 0)
														{
															cout << "String access is out of bounds at beginning" << endl;
														}
														else if((((*pList)[k]) - memDivisor*j) >= config.files[f]->fileStringSize)
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
											catch(exception e)
											{
												cout << "Exception at global index: " << (*pList)[k] << "Exception at relative index: " << ((((*pList)[k]) - memDivisor*j) - (currLevel-1)) << " and computed relative index: " << relativeIndex << " and index for string: " << indexForString << " System exception: " << e.what() << endl;
											}
											k++;
										}
									}
									prevLeafIndex[i] = k;
									justPassedMemorySize = false;
								}
								else
								{
									//if true already do not write again until memory is back in our hands
									if(leaflet.size() > 0 && leaflet[0].leaves.size() > 0)
									{
										justPassedMemorySize = true;
										stringstream stringBuilder;
										stringBuilder << chunkFactorio->GenerateUniqueID();
										fileNamesToReOpen.push_back(chunkFactorio->CreatePartialPatternFile(stringBuilder.str(), leaflet, levelInfo));
									}
									else
									{

										//If memory is unavailable sleep for one second
										//std::this_thread::sleep_for (std::chrono::seconds(1));
									}
									i--;
								}
							}

							//Write remaining patterns to file
							if(leaflet.size() > 0 && leaflet[0].leaves.size() > 0)
							{
								stringstream stringBuilder;
								PListType newID = chunkFactorio->GenerateUniqueID();
								stringBuilder << newID;
								fileNamesToReOpen.push_back(chunkFactorio->CreatePartialPatternFile(stringBuilder.str(), leaflet, levelInfo));
							}

						}
					
						//Deallocate the previous level data
						for(PListType p = 0; p < packedPListArray.size(); p++)
						{
							delete packedPListArray[p];
						}
					}
				}
				archive.CloseArchiveMMAP();
			}

			//Take all of the partial pattern files that were generated above and pull together all patterns to compile full pattern data
			newPatternCount += ProcessChunksAndGenerate(fileNamesToReOpen, newFileNames, memDivisor, threadNum, currLevel, levelInfo.coreIndex);
			
			//Delete processing files
			if(!config.history)
			{
				chunkFactorio->DeletePatternFiles(fileList, ARCHIVE_FOLDER);
			}

			fileList.clear();
			for(int i = 0; i < newFileNames.size(); i++)
			{
				fileList.push_back(newFileNames[i]);
			}
			fileNamesToReOpen.clear();
			newFileNames.clear();

			//Record hd processing statistics
			countMutex->lock();
			stats.SetEradicationsPerLevel(currLevel, stats.GetEradicationsPerLevel(currLevel) + removedPatterns);
			stats.SetEradicatedPatterns(stats.GetEradicatedPatterns() + removedPatterns);
			countMutex->unlock();

			if(newPatternCount > 0 && levelInfo.currLevel < config.maximum)
			{
				levelInfo.currLevel++;
				//Have to add prediction here to see if processing needs to be done in ram or not
				bool prediction = PredictHardDiskOrRAMProcessing(levelInfo, newPatternCount, (config.files[f]->fileStringSize - stats.GetEradicatedPatterns())/(config.numThreads));
				if(!prediction)
				{
					morePatternsToFind = false;
					break;
				}
				else
				{
					
					morePatternsToFind = true;
					//If more threads are available then dispatch multiple threads to process the next level pattern data
					DispatchNewThreadsHD(newPatternCount, morePatternsToFind, fileList, levelInfo, isThreadDefuncted);
				}
			}
			else
			{
				//Delete remaining partial files if not patterns are left to process
				chunkFactorio->DeletePatternFiles(fileList, ARCHIVE_FOLDER);
				morePatternsToFind = false;
			}
		}
	}
	catch(exception e)
	{
		cout << e.what() << endl;
	}

	return newPatternCount;
}

bool Forest::DispatchNewThreadsRAM(PListType newPatternCount, PListType& morePatternsToFind, vector<PListType> &linearList, vector<PListType> &pListLengths, LevelPackage levelInfo, bool& isThreadDefuncted)
{
	bool dispatchedNewThreads = false;
	bool alreadyUnlocked = false;
	countMutex->lock();

	int unusedCores = (config.numThreads - (threadsDispatched - threadsDefuncted)) + 1;
	
	if(pListLengths.size() < unusedCores && unusedCores > 1)
	{
		unusedCores = (int)pListLengths.size();
	}
	//Be conservative with thread allocation
	//Only create new thread for work if the new job will have atleast 10 patterns
	//Stack overflow can occur if there are too many little jobs being assigned
	//Need to have an available core, need to still have patterns to search and need to have more than 1 pattern to be worth splitting up the work
	if(unusedCores > 1 && morePatternsToFind > 0 && pListLengths.size()/unusedCores > 10)
	{
		
		bool spawnThreads = true;
		//If this thread is at the lowest level of progress spawn new threads
		if(spawnThreads)
		{
			//cout << "PList size: " << linearList.size() << " with " << pListLengths.size() << " pattern nodes!" << endl;
			vector<vector<PListType>*>* prevLocalPListArray = new vector<vector<PListType>*>();
			PListType indexing = 0;
			for(int piss = 0; piss < pListLengths.size(); piss++)
			{
				prevLocalPListArray->push_back(new vector<PListType>(linearList.begin() + indexing, linearList.begin() + indexing + pListLengths[piss]));
				indexing += pListLengths[piss];
			}

			linearList.clear();
			linearList.reserve(0);
			pListLengths.clear();
			pListLengths.reserve(0);

			vector<vector<PListType>> balancedTruncList = ProcessThreadsWorkLoadRAM(unusedCores, prevLocalPListArray);
			vector<unsigned int> localWorkingThreads;
			for(unsigned int i = 0; i < balancedTruncList.size(); i++)
			{
				localWorkingThreads.push_back(i);
			}

			if(localWorkingThreads.size() > 1)
			{
				int threadsToTest = (threadsDispatched - threadsDefuncted) - 1;
				if(threadsToTest + localWorkingThreads.size() <= config.numThreads)
				{

					for(int z = 0; z < balancedTruncList.size(); z++)
					{
						unsigned int tally = 0;
						for(int d = 0; d < balancedTruncList[z].size(); d++)
						{
							tally += (unsigned int)(*prevLocalPListArray)[balancedTruncList[z][d]]->size();
						}
					}

					dispatchedNewThreads = true;

					LevelPackage levelInfoRecursion;
					levelInfoRecursion.currLevel = levelInfo.currLevel;
					levelInfoRecursion.threadIndex = levelInfo.threadIndex;
					levelInfoRecursion.inceptionLevelLOL = levelInfo.inceptionLevelLOL + 1;
					levelInfoRecursion.useRAM = true;

					threadsDefuncted++;
					isThreadDefuncted = true;

					vector<future<void>> *localThreadPool = new vector<future<void>>();
					for (PListType i = 0; i < localWorkingThreads.size(); i++)
					{
						threadsDispatched++;
						localThreadPool->push_back(std::async(std::launch::async, &Forest::ThreadedLevelTreeSearchRecursionList, this, prevLocalPListArray, balancedTruncList[i], vector<string>(), levelInfoRecursion));
					}
					countMutex->unlock();

					alreadyUnlocked = true;
					WaitForThreads(localWorkingThreads, localThreadPool, true, levelInfo.threadIndex);

					localThreadPool->erase(localThreadPool->begin(), localThreadPool->end());
					(*localThreadPool).clear();
					delete localThreadPool;
					morePatternsToFind = 0;
					delete prevLocalPListArray;
				}
				else
				{
					for(int piss = 0; piss < prevLocalPListArray->size(); piss++)
					{
						delete (*prevLocalPListArray)[piss];
					}
					delete prevLocalPListArray;
				}
			}
			else
			{
				for(int piss = 0; piss < prevLocalPListArray->size(); piss++)
				{
					delete (*prevLocalPListArray)[piss];
				}
				delete prevLocalPListArray;
			}
		}
		else
		{

		}
	}
	if(!alreadyUnlocked)
	{
		countMutex->unlock();
	}
	return dispatchedNewThreads;
}

bool Forest::DispatchNewThreadsHD(PListType newPatternCount, bool& morePatternsToFind, vector<string> fileList, LevelPackage levelInfo, bool& isThreadDefuncted)
{
	bool dispatchedNewThreads = false;
	bool alreadyUnlocked = false;
	countMutex->lock();

	int unusedCores = (config.numThreads - (threadsDispatched - threadsDefuncted)) + 1;
	if(static_cast<int>(newPatternCount) < unusedCores && unusedCores > 1)
	{
		unusedCores = (int)newPatternCount;
	}
	//Need to have an available core, need to still have patterns to search and need to have more than 1 pattern to be worth splitting up the work
	if(unusedCores > 1 && morePatternsToFind && newPatternCount > 1)
	{
		bool spawnThreads = true;
		//If this thread is at the lowest level of progress spawn new threads
		if(spawnThreads)
		{
			vector<string> tempList = fileList;
			vector<vector<string>> balancedTruncList = ProcessThreadsWorkLoadHD(unusedCores, levelInfo, tempList);
			vector<unsigned int> localWorkingThreads;
			for(unsigned int i = 0; i < balancedTruncList.size(); i++)
			{
				localWorkingThreads.push_back(i);
			}

			if(localWorkingThreads.size() > 1)
			{
				int threadsToTest = (threadsDispatched - threadsDefuncted) - 1;
				if(threadsToTest + localWorkingThreads.size() <= config.numThreads)
				{

					dispatchedNewThreads = true;

					LevelPackage levelInfoRecursion;
					levelInfoRecursion.currLevel = levelInfo.currLevel;
					levelInfoRecursion.threadIndex = levelInfo.threadIndex;
					levelInfoRecursion.inceptionLevelLOL = levelInfo.inceptionLevelLOL + 1;
					levelInfoRecursion.useRAM = false;

					threadsDefuncted++;
					isThreadDefuncted = true;

					vector<future<void>> *localThreadPool = new vector<future<void>>();
					for (PListType i = 0; i < localWorkingThreads.size(); i++)
					{
						threadsDispatched++;
						localThreadPool->push_back(std::async(std::launch::async, &Forest::ThreadedLevelTreeSearchRecursionList, this, prevPListArray, vector<PListType>(), balancedTruncList[i], levelInfoRecursion));

					}
					countMutex->unlock();

					alreadyUnlocked = true;
					WaitForThreads(localWorkingThreads, localThreadPool, true, levelInfo.currLevel);

					localThreadPool->erase(localThreadPool->begin(), localThreadPool->end());
					(*localThreadPool).clear();
					delete localThreadPool;
					morePatternsToFind = false;
				}
			}
		}
	}
	if(!alreadyUnlocked)
	{
		countMutex->unlock();
	}
	return dispatchedNewThreads;
}

PListType Forest::ProcessRAM(vector<vector<PListType>*>* prevLocalPListArray, vector<vector<PListType>*>* globalLocalPListArray, LevelPackage& levelInfo, bool &isThreadDefuncted)
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
	string globalStringConstruct;
	PListType fileSize = config.files[f]->fileStringSize;
	PListType totalCount = 0;
	//Process second level slightly differently than the other levels
	if(levelInfo.currLevel == 2)
	{
		//Take a tally of all the patterns from the previous level
		int threadCountage = 0;
		for (PListType i = 0; i < prevLocalPListArray->size(); i++)
		{
			PListType pListLength = static_cast<PListType>((*prevLocalPListArray)[i]->size());
			if(pListLength > 0)
			{
				totalCount += pListLength;
			}
		}
		//Preallocate
		prevLinearList.reserve(totalCount);

		for (PListType i = 0; i < prevLocalPListArray->size(); i++)
		{
			PListType pListLength = static_cast<PListType>((*prevLocalPListArray)[i]->size());
			//Greater than zero because we look at every thread's contribution to a pattern
			if(pListLength > 0)
			{
				//Flatten out vector of pattern vectors into a flat vector of patterns 
				threadCountage += pListLength;
				prevLinearList.insert(prevLinearList.end(), (*prevLocalPListArray)[i]->begin(), (*prevLocalPListArray)[i]->end());
				delete  (*prevLocalPListArray)[i];
			}
			else
			{
				delete (*prevLocalPListArray)[i];
			}

			//If Hard Disk was processed for the first level then modulo length of pattern
			//based on the number of threads
			if(!firstLevelProcessedHD)
			{
				if(i % config.numThreads == (config.numThreads - 1) && threadCountage > 1)
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
		for (PListType i = 0; i < prevLocalPListArray->size(); i++)
		{
			PListType pListLength = static_cast<PListType>((*prevLocalPListArray)[i]->size());
			if(pListLength > 0)
			{
				totalCount += pListLength;
			}
		}

		//Preallocate
		prevLinearList.reserve(totalCount);

		//Convert vector of pattern vectors into a flat vector with an associated vector of pattern lengths
		for (PListType i = 0; i < prevLocalPListArray->size(); i++)
		{
			PListType pListLength = static_cast<PListType>((*prevLocalPListArray)[i]->size());
			//This is the only pattern information to look at so it must be greater than one instance to be a pattern
			if(pListLength > 1)
			{
				prevLinearList.insert(prevLinearList.end(), (*prevLocalPListArray)[i]->begin(), (*prevLocalPListArray)[i]->end());
				prevPListLengths.push_back(pListLength);
				delete (*prevLocalPListArray)[i];
			}
			else
			{
				delete (*prevLocalPListArray)[i];
			}
		}
	}
	//Now allocate memory to eventually load pattern information into a contiguous memory vector instead of hopping around the file by indexing into it
	//based on the patterns
	globalStringConstruct.resize(totalCount);
	globalStringConstruct.shrink_to_fit();
	linearList.reserve(totalCount);

	PListType linearListIndex = 0;

	//We have nothing to process!
	if(totalCount == 0)
		return false;

	while(continueSearching > 0)
	{
		
		totalTallyRemovedPatterns = 0;
		PListType minIndex = -1;
		PListType maxIndex = 0;
		PListType stringIndexer = 0;

		//Prep all pattern information for next level processing
		if(levelInfo.currLevel == 2)
		{
			PListType linearListSize = static_cast<PListType>(prevLinearList.size());
			if(linearListSize > 0)
			{
				for (PListType i = 0; i < linearListSize; i++)
				{
					if (prevLinearList[i] < fileSize)
					{
						globalStringConstruct[stringIndexer++] = config.files[f]->fileString[prevLinearList[i]];
					}
				}
			}
		}
		else
		{
			PListType linearListSize = static_cast<PListType>(prevLinearList.size());
			if(linearListSize > 1)
			{
				for (PListType i = 0; i < linearListSize; i++)
				{
					if (prevLinearList[i] < fileSize)
					{
						globalStringConstruct[stringIndexer++] = config.files[f]->fileString[prevLinearList[i]];
					}
				}
			}
		}

		if(prevPListLengths.size() == 0)
		{
			continueSearching = 0;
			break;
		}

		//Shrink vector to be correct size
		globalStringConstruct.resize(stringIndexer);
		globalStringConstruct.shrink_to_fit();
		stringIndexer = 0;
		vector<PListType> newPList[256];

		//Begin pattern processing with the global string data used for quicker processing
		if(levelInfo.currLevel == 2)
		{
			//Use 256 stack vectors because one previous level pattern can only grow into 256 new pattern possibilites 
			PListType prevPListSize = static_cast<PListType>(prevLinearList.size());
			PListType indexes[256] = {0};
			PListType indexesToPush[256] = {0};
			PListType firstPatternIndex[256] = {0};
			int listLength = 0;
			int currList = 0;
			int currPosition = 0;
			int limit = prevPListLengths[currList];
			for (PListType i = 0; i < prevPListSize; i++)
			{
				//If pattern is past end of string stream then stop counting this pattern
				PListType index = prevLinearList[i];
				if (index < fileSize)
				{
					uint8_t indexIntoFile = (uint8_t)globalStringConstruct[stringIndexer++];
					if(firstPatternIndex[indexIntoFile])
					{
						if(newPList[indexIntoFile].empty())
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
					totalTallyRemovedPatterns++;
				}

				if(i + 1 == currPosition + limit)
				{
					for (int k = 0; k < listLength; k++)
					{
						PListType insert = static_cast<PListType>(indexes[indexesToPush[k]]);
						if (insert >= config.minOccurrence)
						{
							//Process and eliminate patterns if they overlap each other
							if(config.nonOverlappingPatternSearch == NONOVERLAP_PATTERNS)
							{
								//Monitor number of patterns that do not overlap ie coverage
								PListType prevIndex = *newPList[indexesToPush[k]].begin();
								PListType totalTally = 1;

								linearList.push_back(prevIndex);
								for(vector<PListType>::iterator it = newPList[indexesToPush[k]].begin() + 1; it != newPList[indexesToPush[k]].end(); it++)
								{
									PListType span = *it - prevIndex;
									if(span >= levelInfo.currLevel)
									{
										totalTally++;
										linearList.push_back(*it);
										prevIndex = *it;
									}
									else
									{
										totalTallyRemovedPatterns++;
									}
								}
								if(newPList[indexesToPush[k]].size() == 2 && totalTally == 1)
								{
									totalTallyRemovedPatterns++;
									linearList.pop_back();
								}
								else
								{
									pListLengths.push_back(totalTally);
								}
							}
							//Process all patterns because patterns CAN overlap each other
							else if(config.nonOverlappingPatternSearch == OVERLAP_PATTERNS)
							{
								//Monitor number of patterns that do not overlap ie coverage
								PListType prevIndex = *newPList[indexesToPush[k]].begin();
								PListType totalTally = 1;

								linearList.push_back(prevIndex);
								for(vector<PListType>::iterator it = newPList[indexesToPush[k]].begin() + 1; it != newPList[indexesToPush[k]].end(); it++)
								{
									PListType span = *it - prevIndex;
									if(span < levelInfo.currLevel)
									{
										totalTally++;
										linearList.push_back(*it);
										prevIndex = *it;
									}
									else
									{
										prevIndex = *it;
										totalTallyRemovedPatterns++;
									}
								}
								if(newPList[indexesToPush[k]].size() == 2 && totalTally == 1)
								{
									totalTallyRemovedPatterns++;
									linearList.pop_back();
								}
								else
								{
									pListLengths.push_back(totalTally);
								}
							}
							else
							{
								pListLengths.push_back(static_cast<PListType>(newPList[indexesToPush[k]].size()));
								linearList.insert(linearList.end(), newPList[indexesToPush[k]].begin(), newPList[indexesToPush[k]].end());
							}

							indexes[indexesToPush[k]] = 0;
							firstPatternIndex[indexesToPush[k]] = 0;
							newPList[indexesToPush[k]].clear();
						}
						//If only one instance of a sequence of data then discard the previous pattern data
						else if(insert == 1)
						{
							totalTallyRemovedPatterns++;
							indexes[indexesToPush[k]] = 0;
							firstPatternIndex[indexesToPush[k]] = 0;
							newPList[indexesToPush[k]].clear();
						}

					}
					if(currList + 1 < prevPListLengths.size())
					{
						currPosition = i + 1;
						currList++;
						limit = prevPListLengths[currList];
						listLength = 0;
					}
				}
			}
		}
		//Process for levels 3 and greater
		else
		{
			PListType prevPListSize = static_cast<PListType>(prevLinearList.size());
			PListType indexes[256] = {0};
			PListType indexesToPush[256] = {0};
			PListType firstPatternIndex[256] = {0};
			int listLength = 0;

			int currList = 0;
			int currPosition = 0;
			int limit = prevPListLengths[currList];
			for (PListType i = 0; i < prevPListSize; i++)
			{
				//If pattern is past end of string stream then stop counting this pattern
				PListType index = prevLinearList[i];
				if (index < fileSize)
				{
					uint8_t indexIntoFile = (uint8_t)globalStringConstruct[stringIndexer++];
					if(firstPatternIndex[indexIntoFile])
					{
						if(newPList[indexIntoFile].empty())
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
					totalTallyRemovedPatterns++;
				}

				if(i + 1 == currPosition + limit)
				{
					for (int k = 0; k < listLength; k++)
					{
						PListType insert = static_cast<PListType>(indexes[indexesToPush[k]]);
						if (insert >= config.minOccurrence)
						{
							//Process and eliminate patterns if they overlap each other
							if(config.nonOverlappingPatternSearch == NONOVERLAP_PATTERNS)
							{
								//Monitor number of patterns that do not overlap ie coverage
								PListType prevIndex = *newPList[indexesToPush[k]].begin();
								PListType totalTally = 1;

								linearList.push_back(prevIndex);
								for(vector<PListType>::iterator it = newPList[indexesToPush[k]].begin() + 1; it != newPList[indexesToPush[k]].end(); it++)
								{
									PListType span = *it - prevIndex;
									if(span >= levelInfo.currLevel)
									{
										totalTally++;
										linearList.push_back(*it);
										prevIndex = *it;
									}
									else
									{
										totalTallyRemovedPatterns++;
									}
								}
								if(newPList[indexesToPush[k]].size() == 2 && totalTally == 1)
								{
									totalTallyRemovedPatterns++;
									linearList.pop_back();
								}
								else
								{
									pListLengths.push_back(totalTally);
								}
							}
							//Process all patterns because patterns CAN overlap each other
							else if(config.nonOverlappingPatternSearch == OVERLAP_PATTERNS)
							{
								//Monitor number of patterns that do not overlap ie coverage
								PListType prevIndex = *newPList[indexesToPush[k]].begin();
								PListType totalTally = 1;

								linearList.push_back(prevIndex);
								for(vector<PListType>::iterator it = newPList[indexesToPush[k]].begin() + 1; it != newPList[indexesToPush[k]].end(); it++)
								{
									PListType span = *it - prevIndex;
									if(span < levelInfo.currLevel)
									{
										totalTally++;
										linearList.push_back(*it);
										prevIndex = *it;
									}
									else
									{
										prevIndex = *it;
										totalTallyRemovedPatterns++;
									}
								}
								if(newPList[indexesToPush[k]].size() == 2 && totalTally == 1)
								{
									totalTallyRemovedPatterns++;
									linearList.pop_back();
								}
								else
								{
									pListLengths.push_back(totalTally);
								}
							}
							else
							{
								pListLengths.push_back(static_cast<PListType>(newPList[indexesToPush[k]].size()));
								linearList.insert(linearList.end(), newPList[indexesToPush[k]].begin(), newPList[indexesToPush[k]].end());
							}

							indexes[indexesToPush[k]] = 0;
							firstPatternIndex[indexesToPush[k]] = 0;
							newPList[indexesToPush[k]].clear();
						}
						else if(insert == 1)
						{
							totalTallyRemovedPatterns++;
							indexes[indexesToPush[k]] = 0;
							firstPatternIndex[indexesToPush[k]] = 0;
							newPList[indexesToPush[k]].clear();
						}

					}
					if(currList + 1 < prevPListLengths.size())
					{
						currPosition = i + 1;
						currList++;
						limit = prevPListLengths[currList];
						listLength = 0;
					}
				}
			}
		}

		//Populate level statistics including most common pattern and instance of that pattern and how many patterns were found
		countMutex->lock();

		//Add all of the pattern counts
		for(PListType i : pListLengths)
		{
			stats.SetTotalOccurrenceFrequency(levelInfo.currLevel, i);
		}

		//If levelToOutput is not selected but -Pall is set or if -Pall is set and -Plevel is set to output data only for a specific level
		if(config.levelToOutput == 0 || config.levelToOutput == levelInfo.currLevel)
		{
			//Keeps track of the index in pListLengths vector
			vector<PListType> positionsInLinearList(pListLengths.size());
			PListType pos = 0;
			for(PListType i = 0; i < positionsInLinearList.size(); i++)
			{
				positionsInLinearList[i] = pos;
				pos += pListLengths[i];
			}
			
			for(PListType z = 0; z < pListLengths.size(); z++)
			{
				PListType distances = 0;
				PListType index = positionsInLinearList[z];
				PListType length = pListLengths[z];
				PListType coverageSubtraction = 0;
				//Calculate average distance between pattern instances
				for(int i = index; i < index + length - 1; i++)
				{
					distances += linearList[i+1] - linearList[i];
					if(linearList[i+1] - linearList[i] < levelInfo.currLevel)
					{
						coverageSubtraction += levelInfo.currLevel - (linearList[i+1] - linearList[i]);
					}
				}
				float averageDistance = ((float)distances)/((float)(length - 1));
				stringstream data;

				//Struct used to contain detailed pattern information for one level
				ProcessorStats::DisplayStruct outputData;
				outputData.patternInstances = length;
				outputData.patternCoveragePercentage = (float)100.0f*(((length*levelInfo.currLevel) - coverageSubtraction))/(float)config.files[f]->fileStringSize;
				outputData.averagePatternDistance = averageDistance;
				outputData.firstIndexToPattern = linearList[index];
				
				//If pnoname is not selected then strings are written to log, this could be for reasons where patterns are very long
				if(!config.suppressStringOutput)
				{
					outputData.pattern = config.files[f]->fileString.substr(linearList[index] - levelInfo.currLevel, levelInfo.currLevel);
				}
				stats.detailedLevelInfo.push_back(outputData);
			}
		}

		stats.SetEradicationsPerLevel(levelInfo.currLevel, stats.GetEradicationsPerLevel(levelInfo.currLevel) + totalTallyRemovedPatterns);
		stats.SetEradicatedPatterns(stats.GetEradicatedPatterns() + totalTallyRemovedPatterns);
		stats.SetLevelRecording(levelInfo.currLevel, stats.GetLevelRecording(levelInfo.currLevel) + static_cast<PListType>(pListLengths.size()));

		PListType tempMostCommonPatternCount = stats.GetMostCommonPatternCount(levelInfo.currLevel);
		PListType tempMostCommonPatternIndex = 0;

		stats.SetCurrentLevel(levelInfo.threadIndex, levelInfo.currLevel + 1);
		
		countMutex->unlock();

		PListType countage = 0;
		PListType indexOfList = 0;
		bool chosen = false;
		PListType unalteredCount = 0;
		PListType indexToDistance = 0;
		PListType distanceLength = 0;
		for (PListType i = 0; i < pListLengths.size(); i++)
		{
			if(pListLengths[i] > 1)
			{
				PListType prevIndex = linearList[countage];
				PListType tallyCount = 1;

				for(PListType j = countage + 1; j < pListLengths[i] + countage; j++)
				{
					PListType span = linearList[j] - prevIndex;
					if(span >= levelInfo.currLevel)
					{
						prevIndex = linearList[j];
						tallyCount++;
					}
				}
				if( tallyCount > tempMostCommonPatternCount)
				{
					tempMostCommonPatternCount = tallyCount;
					unalteredCount = pListLengths[i];
					indexToDistance = countage;
					distanceLength = pListLengths[i];
					tempMostCommonPatternIndex = linearList[countage] - levelInfo.currLevel;
					indexOfList = countage;
					chosen = true;
				}
			}
			countage += pListLengths[i];
		}

		//If this level contains the most common pattern that add it
		if(chosen)
		{
			countMutex->lock();
			stats.SetMostCommonPattern(levelInfo.currLevel, tempMostCommonPatternCount, tempMostCommonPatternIndex);
			PListType distances = 0;
			for(PListType j = indexToDistance; j < indexToDistance + distanceLength - 1; j++)
			{
				distances += linearList[j+1] - linearList[j];
			}
			float averageDistance = ((float)distances)/((float)(distanceLength - 1));
			stats.SetDistance(levelInfo.currLevel, averageDistance);
			countMutex->unlock();

			//Monitor number of patterns that do not overlap ie coverage
			PListType index = indexOfList;
			countMutex->lock();
			PListType count = unalteredCount;
			countMutex->unlock();
			PListType totalTally = 0;
			float percentage = 0;
			PListType prevIndex = 0;
			PListType totalCoverage = 0;
			if(count > 1)
			{
				prevIndex = linearList[index];
				totalTally++;
				totalCoverage += levelInfo.currLevel;
		
				for(PListType i = index + 1; i < count + index; i++)
				{
					PListType span = linearList[i] - prevIndex;
					if(span >= levelInfo.currLevel)
					{
						PListType pIndex = linearList[i];
						totalTally++;
						prevIndex = pIndex;
						totalCoverage += levelInfo.currLevel;
					}
				}
				//Coverage of most common pattern per level
				percentage = ((float)(totalCoverage)) / ((float)(config.files[f]->fileStringSize));
			}

			//Set coverage of pattern at this level
			countMutex->lock();
			stats.SetCoverage(levelInfo.currLevel, percentage);
			countMutex->unlock();

		}

		levelInfo.currLevel++;

		//Swap previous and next level data for the proceeding level to process
		if(linearList.size() == 0 || levelInfo.currLevel - 1 >= config.maximum)
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
			bool prediction = PredictHardDiskOrRAMProcessing(levelInfo, static_cast<PListType>(pListLengths.size()), static_cast<PListType>(linearList.size()));
			
			//If we need to switch gears and process using the hard disk for this proceeding level
			if(prediction)
			{

				PListType indexing = 0;
				prevLocalPListArray->clear();
				for(int i = 0; i < pListLengths.size(); i++)
				{
					prevLocalPListArray->push_back(new vector<PListType>(linearList.begin() + indexing, linearList.begin() + indexing + pListLengths[i]));
					indexing += pListLengths[i];
				}

				continueSearching = static_cast<PListType>(pListLengths.size());

				linearList.clear();
				linearList.reserve(0);
				pListLengths.clear();
				pListLengths.reserve(0);
				break;
			}
			//If still using RAM we can attempt to dispatch more processing threads for this proceeding level
			else
			{
				continueSearching = static_cast<PListType>(pListLengths.size());
				
				DispatchNewThreadsRAM(0, continueSearching, linearList, pListLengths, levelInfo, isThreadDefuncted);

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
	return continueSearching;
}

void Forest::ThreadedLevelTreeSearchRecursionList(vector<vector<PListType>*>* patterns, vector<PListType> patternIndexList, vector<string> fileList, LevelPackage levelInfo)
{
	//Grab the number of patterns to search with
	PListType numPatternsToSearch = static_cast<PListType>(patternIndexList.size());
	bool isThreadDefuncted = false;

	//Make sure to not use too many threads
	if(threadsDispatched - threadsDefuncted > static_cast<int>(config.numThreads))
	{
		cout << "WENT OVER THREADS ALLOCATION SIZE!" << endl;
	}

	//Vectors used for previous and next level storage
	vector<vector<PListType>*>* prevLocalPListArray = new vector<vector<PListType>*>();
	vector<vector<PListType>*>*	globalLocalPListArray = new vector<vector<PListType>*>();

	//If the previous level was processed using RAM then push data into the previous vector
	if(levelInfo.useRAM)
	{
		for(PListType i = 0; i < numPatternsToSearch; i++)
		{
			if((*patterns)[patternIndexList[i]] != NULL)
			{
				prevLocalPListArray->push_back((*patterns)[patternIndexList[i]]);
			}
		}
	}

	PListType continueSearching = 1;
	bool processingRAM = false;
	bool useRAMBRO = levelInfo.useRAM;
	while(continueSearching > 0)
	{
		//If not the second level of processing then do a prediction
		//because for level 2 the prediction is already done using levelInfo
		if(levelInfo.currLevel != 2)
		{
			if(!config.usingPureHD && !config.usingPureRAM)
			{
				//Compute processing prediction 
				bool prediction = processingRAM ? true : false;
				PrepData(prediction, levelInfo, fileList, prevLocalPListArray);
				useRAMBRO = !prediction;
			}
		}
		else
		{
			useRAMBRO = levelInfo.useRAM;
		}

		//If RAM is predicted to be used then dispatch processing the previous level using RAM
		if(useRAMBRO)
		{
			continueSearching = ProcessRAM(prevLocalPListArray, globalLocalPListArray, levelInfo, isThreadDefuncted);
			processingRAM = true;
		}
		//If Hard Disk is predicted to be used then dispatch processing the previous level using Hard Disk
		else
		{
			continueSearching = ProcessHD(levelInfo, fileList, isThreadDefuncted);
			processingRAM = false;
		}
	}

	//When processing is finished clean up vectors
	if(prevLocalPListArray != NULL)
	{
		delete prevLocalPListArray;
	}
	if(globalLocalPListArray != NULL)
	{
		delete globalLocalPListArray;
	}

	//Defunct the thread so other threads can use more processing power
	//this essentially lets other threads know there is an available thread for them
	countMutex->lock();
	if(!isThreadDefuncted)
	{
		threadsDefuncted++;
	}
	countMutex->unlock();
}

void Forest::PlantTreeSeedThreadHD(PListType positionInFile, PListType startPatternIndex, PListType numPatternsToSearch, unsigned int threadNum)
{
	LevelPackage levelInfo;
	levelInfo.currLevel = 1;
	levelInfo.coreIndex = threadNum;
	levelInfo.threadIndex = threadNum;

	//Populate vector with 256 entries for the first level processing which can only have up to 256 patterns
	//and make an early approximation for vector sizes to prevent as much reallocations as possible
	PListType earlyApproximation = static_cast<PListType>(config.files[f]->fileString.size()/(256*(config.numThreads)));
	vector<vector<PListType>*> leaves(256);
	for(int i = 0; i < 256; i++)
	{
		leaves[i] = new vector<PListType>();
		leaves[i]->reserve(earlyApproximation);
	}

	PListType counting = 0;

	for (PListType i = startPatternIndex; i < numPatternsToSearch + startPatternIndex; i++)
	{
		//Index into the file plus an offset if processing file in partitions
		int temp = i + positionInFile + 1;
		uint8_t tempIndex = (uint8_t)config.files[f]->fileString[i];
		if(config.patternToSearchFor.size() == 0 || config.files[f]->fileString[i] == config.patternToSearchFor[0])
		{
			leaves[tempIndex]->push_back(temp);
			counting++;
		}
		//If RAM memory is allocated over the limit then dump the patterns to file
		if(overMemoryCount && counting >= PListArchive::hdSectorSize)
		{
			stringstream stringBuilder;
			PListType newID = chunkFactorio->GenerateUniqueID();
			stringBuilder << newID;
			newFileNameList[threadNum].push_back(chunkFactorio->CreatePartialPatternFile(stringBuilder.str(), leaves, levelInfo));
			//Reallocate vectors for storage
			for(int i = 0; i < 256; i++)
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
	for(int i = 0; i < 256; i++)
	{
		leaves[i] = new vector<PListType>();
	}

	//Last index in the file that will be processed, creating a range to search in
	PListType endPatternIndex = numPatternsToSearch + startPatternIndex;

	for (PListType i = startPatternIndex; i < endPatternIndex; i++)
	{
		//Index into the file plus an offset if processing file in partitions
		int temp = i + positionInFile + 1;
		uint8_t tempIndex = (uint8_t)config.files[f]->fileString[i];

		//If ranges are set only search for patterns in the specified range
		if(config.lowRange == config.highRange || tempIndex >= config.lowRange && tempIndex <= config.highRange)
		{
			leaves[tempIndex]->push_back(temp);
		}
	}

	//Push new patterns into the shared vector that will be pulled together with the other processing threads
	for(int i = 0; i < 256; i++)
	{
		(*prevPListArray)[threadIndex + i*config.numThreads] = leaves[i];
	}

	//Set level to 2 because level 1 processing is finished
	stats.SetCurrentLevel(threadIndex, 2);
}
