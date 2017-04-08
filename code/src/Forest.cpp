#include "Forest.h"
#include "MemoryUtils.h"
#include <locale>
#include <list>
#include <algorithm>
#include <signal.h>

bool Forest::overMemoryCount = false;
Forest::Forest(int argc, char **argv)
{

#if defined(_WIN64) || defined(_WIN32)
	//Hard code page size to 2 MB for windows
	PListArchive::hdSectorSize = 2097152;//4096;
	system("del ..\\..\\Log\\PList*.txt");
#elif defined(__linux__)
	PListArchive::hdSectorSize = sysconf (_SC_PAGESIZE);
	system("rm -r ../Log/PList*");
#endif

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
	int threadsToDispatch = 0;
	if(config.findBestThreadNumber)
	{
		config.numThreads = 2;
	}
	threadsToDispatch = config.numThreads - 1;


	memoryCeiling = (PListType)MemoryUtils::GetAvailableRAMMB() - 1000;

	//If memory bandwidth not an input
	if(!config.usingMemoryBandwidth)
	{

		//Leave 1 GB to spare for operating system in case our calculations suck
		config.memoryBandwidthMB = memoryCeiling - 1000;
	}

	thread *memoryQueryThread = NULL;
	
	double threadMemoryConsumptionInMB = MemoryUtils::GetProgramMemoryConsumption();
	stringstream crappy;
	crappy << "Errant memory after processing level " << threadMemoryConsumptionInMB - MemoryUsageAtInception << " in MB!\n";
	Logger::WriteLog(crappy.str());

	vector<map<int, double>> threadMap;
	
	threadPool = new vector<future<void>>();
	
	for(f = 0; f < config.files.size(); f++)
	{
		PListType earlyApproximation = config.files[f]->fileString.size()/(256);

		const char * c = config.files[f]->fileName.c_str();
	
		// Open the file for the shortest time possible.

		config.files[f]->copyBuffer = new ifstream(c, ios::binary);

		if (!config.files[f]->copyBuffer->is_open()) 
		{
			//Close file handle once and for all
			config.files[f]->copyBuffer->clear();
			config.files[f]->fileString.clear();
			config.files[f]->fileString.reserve(0);

			delete config.files[f];
			continue;
		}

		if(config.files[f]->fileString.size() == 0 && config.usingPureRAM)
		{
			//new way to read in file
			config.files[f]->fileString.clear();
			config.files[f]->fileString.resize(config.files[f]->fileStringSize);
			config.files[f]->copyBuffer->read( &config.files[f]->fileString[0], config.files[f]->fileString.size());
		}

		
		for(unsigned int threadIteration = 0; threadIteration <= config.testIterations; threadIteration = threadsToDispatch)
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

			initTime.Start();

			prevFileNameList.clear();
			newFileNameList.clear();
			prevFileNameList.resize(config.numThreads - 1);
			newFileNameList.resize(config.numThreads - 1);

			fileChunks.clear();
			fileChunks.reserve(0);

			chunkIndexToFileChunk.clear();

			//Kick off thread that processes how much memory the program uses at a certain interval
			memoryQueryThread = new thread(&Forest::MemoryQuery, this);
			
			//Initialize all possible values for the first list to NULL
			prevPListArray->resize(256*threadsToDispatch);
			for(int i = 0; i < 256*threadsToDispatch; i++)
			{
				(*prevPListArray)[i] = NULL;
			}

			//Assume start with RAM
			usedRAM.resize(threadsToDispatch);
			for(int i = 0; i < threadsToDispatch; i++)
			{
				usedRAM[i] = true;
			}

			//currentLevelVector.resize(threadsToDispatch);
			activeThreads.resize(threadsToDispatch);
			for(int i = 0; i < threadsToDispatch; i++)
			{
				activeThreads[i] = false;
			}

			memoryPerThread = config.memoryBandwidthMB/threadsToDispatch;
			cout << "Memory that can be used per thread: " << memoryPerThread << " MB." << endl;

			
			PListType overallFilePosition = 0;

			//make this value 1 so calculations work correctly then reset
			LevelPackage levelInfo;
			levelInfo.currLevel = 1;
			levelInfo.inceptionLevelLOL = 0;
			levelInfo.threadIndex = 0;
			levelInfo.useRAM = usedRAM[0];
			levelInfo.coreIndex = 0;
			bool prediction = PredictHardDiskOrRAMProcessing(levelInfo, 1, config.files[f]->fileStringSize);
			firstLevelProcessedHD = prediction;
			for(int i = 0; i < threadsToDispatch; i++)
			{
				usedRAM[i] = !prediction;
				stats.SetCurrentLevel(i, 1);
			}


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

			cout << "Number of threads processing file is " << threadsToDispatch << endl;
			for(int z = 0; z < fileIterations; z++)
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

				
			
				PListType cycles = patternCount/threadsToDispatch;
				PListType lastCycle = patternCount - (cycles*threadsToDispatch);
				PListType span = cycles;

				
				for(int i = 0; i < threadsToDispatch; i++)
				{
					if(!(i < threadsToDispatch - 1))
					{
						span = span + lastCycle;
					}	
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
				for(unsigned int i = 0; i < threadsToDispatch; i++)
				{
					localWorkingThreads.push_back(i);
				}
				WaitForThreads(localWorkingThreads, threadPool); 

				if(!prediction)
				{
					PListType indexOfList = 0;
					std::map<string, PListType> countMap;
					std::map<string, vector<PListType>> indexMap;
					for (PListType i = 0; i < prevPListArray->size(); i++)
					{
						if((*prevPListArray)[i] != nullptr && (*prevPListArray)[i]->size() > 0)
						{
							countMap[config.files[f]->fileString.substr((*(*prevPListArray)[i])[0] - (levelInfo.currLevel), levelInfo.currLevel)] += (*prevPListArray)[i]->size();
							
							if( countMap[config.files[f]->fileString.substr((*(*prevPListArray)[i])[0] - (levelInfo.currLevel), levelInfo.currLevel)] > stats.GetMostCommonPatternCount(levelInfo.currLevel))
							{
								stats.SetMostCommonPattern(levelInfo.currLevel, 
									countMap[config.files[f]->fileString.substr((*(*prevPListArray)[i])[0] - (levelInfo.currLevel), levelInfo.currLevel)],
									(*(*prevPListArray)[i])[0] - (levelInfo.currLevel));

								indexOfList = i;
							}

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

					stats.SetLevelRecording(levelInfo.currLevel, countMap.size() - removedPatterns);

					//Coverage cannot overlap on the first level by definition
					stats.SetCoverage(levelInfo.currLevel, ((double)(stats.GetMostCommonPatternCount(levelInfo.currLevel))) / ((double)(config.files[f]->fileStringSize)));
					
					stats.SetEradicationsPerLevel(levelInfo.currLevel, stats.GetEradicationsPerLevel(levelInfo.currLevel) + removedPatterns);
					stats.SetEradicatedPatterns(stats.GetEradicatedPatterns() + removedPatterns);

				}
				overallFilePosition += position;

				threadPool->erase(threadPool->begin(), threadPool->end());
				(*threadPool).clear();
				
				if(prediction)
				{
					for(int z = 0; z < threadsToDispatch; z++)
					{
						for(int a = 0; a < newFileNameList[z].size(); a++)
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

			//Divide between file load and previous level pLists and leave some for new lists haha 
			PListType memDivisor = (PListType)((memoryPerThread*1000000)/3.0f);

			int currentFile = 0;
			bool memoryOverflow = false;
			vector<string> temp;
			if(prediction)
			{
				if(config.levelToOutput == 0 || config.levelToOutput != 0)
				{
					ProcessChunksAndGenerateLargeFile(backupFilenames, temp, memDivisor, 0, 1, true);
				}
			}

			//Start searching
			if(2 <= config.maximum)
			{
				NextLevelTreeSearch(2);
			}

			time.Stop();
			threadMap.push_back(map<int, double>());
			threadMap[f][threadsToDispatch] = time.GetTime();
			processingTimes.push_back(threadMap[f][threadsToDispatch]);
			time.Display();
			stringstream buffery;
			buffery << threadsToDispatch << " threads were used to process file" << endl;
			Logger::WriteLog(buffery.str());

			if(config.files[f]->fileStringSize != config.files[f]->fileString.size())
			{
				config.files[f]->fileString.clear();
				config.files[f]->copyBuffer->seekg( 0 );
				config.files[f]->fileString.resize(config.files[f]->fileStringSize);
				config.files[f]->copyBuffer->read( &config.files[f]->fileString[0], config.files[f]->fileString.size());
			}

			if(Logger::verbosity)
			{
				for(int j = 0; j < stats.GetLevelRecordingSize() && stats.GetLevelRecording(j + 1) != 0; j++)
				{
					string pattern = config.files[f]->fileString.substr(stats.GetMostCommonPatternIndex(j + 1), j + 1);
					stringstream buff;
					buff << "Level " << j + 1 << " count is " << stats.GetLevelRecording(j + 1) << " with most common pattern being: \"" << pattern << "\"" << " occured " << stats.GetMostCommonPatternCount(j + 1) << " and coverage was " << stats.GetCoverage(j + 1) << "% " << "eradicated patterns " << stats.GetEradicationsPerLevel(j + 1) << endl;
					Logger::WriteLog(buff.str());
				}
				
				/*string pattern = config.files[f]->fileString.substr(mostCommonPatternIndex[levelRecordings.size() - 2], levelRecordings.size());
				stringstream buff;
				buff << "Level " << levelRecordings.size() << " count is " << levelRecordings[levelRecordings.size() - 2] << " with most common pattern being: \"" << pattern << "\"" << " occured " << GetMostCommonPatternIndex([levelRecordings.size() - 1) << " and coverage was " << coverage[levelRecordings.size() - 2] << "%" << endl;
				Logger::WriteLog(buff.str());*/
				
			}

			//Logger::fillPatternData(files[f]->fileString, mostCommonPatternIndex);
			Logger::fileCoverageCSV(stats.GetCoverageVector());

			finalPattern[stats.GetLevelRecordingSize()]++;

			for (int i = 0; i < prevPListArray->size(); i++)
			{
				if((*prevPListArray)[i] != NULL)
				{
					delete (*prevPListArray)[i];
				}
			}
			prevPListArray->clear();

			if(config.findBestThreadNumber)
			{
				config.numThreads = (config.numThreads * 2) - 1;
				threadsToDispatch = config.numThreads - 1;
			}

			crappy.str("");
			crappy << "File Size " << config.files[f]->fileStringSize << " and eliminated patterns " << stats.GetEradicatedPatterns() << "\n\n\n";
			Logger::WriteLog(crappy.str());

			//If we aren't doing a deep search in levels then there isn't a need to check that pattern finder is properly functioning..it's impossible
			if(config.files[f]->fileStringSize != stats.GetEradicatedPatterns() && config.maximum == -1)
			{
				cout << "Houston we are not processing patterns properly!" << endl;
				Logger::WriteLog("Houston we are not processing patterns properly!");
				exit(0);
			}

			if(memoryQueryThread != NULL)
			{
				processingFinished = true;
				memoryQueryThread->join();
				delete memoryQueryThread;
				memoryQueryThread = NULL;
				processingFinished = false;
			}
			stats.ResetData();
		}
		
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
			config.numThreads = 2;
			threadsToDispatch = config.numThreads - 1;
		}

	}

	Logger::generateTimeVsFileSizeCSV(processingTimes, config.fileSizes);

	Logger::generateFinalPatternVsCount(finalPattern);

	if(config.findBestThreadNumber)
	{
		Logger::generateThreadsVsThroughput(threadMap);
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
		if(currMemoryOverflow + config.memoryBandwidthMB > memoryCeiling)
		{
			Logger::WriteLog("Have to bail because you are using too much memory for your system!");
			exit(0);
		}

		if(swTimer.GetTime() > 10000.0f)
		{
			PListType timeStamp = swTimer.GetTime();
			loggingIt.str("");
			swTimer.Start();
			loggingIt << "Thread level status...\n";
			for(int j = 0; j < stats.GetCurrentLevelSize(); j++)
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
	
	//main thread is a hardware thread so dispatch threads requested minus 1
	PListType threadsToDispatch = config.numThreads - 1;

	
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
		sizeOfProcessedFile = config.files[f]->fileStringSize/threadsToDispatch;
	}

	PListType sizeOfGlobalStringConstruct = sizeOfString;
	PListType totalStorageNeeded = (linearListPListLengthsContainerSizesForPrevAndNext + sizeOfProcessedFile + sizeOfGlobalStringConstruct)/1000000.0f;

	PListType previousLevelMemoryMB = 0;

	double prevMemoryMB = MemoryUtils::GetProgramMemoryConsumption() - MemoryUsageAtInception;
	if(prevMemoryMB > 0.0f)
	{
		previousLevelMemoryMB = (PListType)prevMemoryMB/threadsToDispatch;
	}

	PListType memoryAllowance = 0;
	if(levelInfo.currLevel <= 2)
	{
		memoryAllowance = config.memoryBandwidthMB;
	}
	else
	{
		memoryAllowance = config.memoryBandwidthMB/threadsToDispatch;
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
			//new way to read in file
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
	PListType threadsToDispatch = config.numThreads - 1;
	vector<vector<string>> tempFileList = fileList;
	for(int a = 0; a < fileList.size(); a++)
	{
		fileList[a].clear();
	}

	if(prediction)
	{
		if(!usedRAM[0])
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
					if(threadNumber >= threadsToDispatch)
					{
						threadNumber = 0;
					}
				}
			}
		}
		else if(usedRAM[0])
		{
			//chunk files
			vector<PListArchive*> threadFiles;
			stringstream threadFilesNames;
			unsigned int threadNumber = 0;

			for(int a = 0; a < threadsToDispatch; a++)
			{
				threadFilesNames.str("");
				threadFilesNames << "PListChunks" << chunkFactorio->GenerateUniqueID();

				threadFiles.push_back(new PListArchive(threadFilesNames.str(), true));
				fileList[a].push_back(threadFilesNames.str());
			}
			for(PListType prevIndex = 0; prevIndex < prevLocalPListArray->size(); )
			{
				list<PListType> *sorting = new list<PListType>();

				for(int threadCount = 0; threadCount < threadsToDispatch; threadCount++)
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
				if(threadNumber >= threadsToDispatch)
				{
					threadNumber = 0;
				}
			}
			//Clear out the array also after deletion
			prevLocalPListArray->clear();

			for(int a = 0; a < threadsToDispatch; a++)
			{
				threadFiles[a]->WriteArchiveMapMMAP(vector<PListType>(), "", true);
				threadFiles[a]->CloseArchiveMMAP();
				delete threadFiles[a];
			}
		}
	}
	else if(!prediction)
	{
		if(!usedRAM[0])
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

			for(PListType i = 0; i < threadsToDispatch; i++)
			{
				if(!config.history)
				{
					chunkFactorio->DeleteArchives(tempFileList[i], ARCHIVE_FOLDER);
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

	for(int a = 0; a < usedRAM.size(); a++)
	{
		usedRAM[a] = !prediction;
	}
}

void Forest::PrepData(bool prediction, LevelPackage& levelInfo, vector<string>& fileList, vector<vector<PListType>*>* prevLocalPListArray, vector<vector<PListType>*>* globalLocalPListArray)
{
	PListType threadsToDispatch = config.numThreads - 1;

	if(prediction)
	{
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
	else if(!prediction)
	{
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
				chunkFactorio->DeleteArchives(fileList, ARCHIVE_FOLDER);
			}
			fileList.clear();
		}
	}

	levelInfo.useRAM = !prediction;
	usedRAM[levelInfo.threadIndex] = !prediction;

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

	unsigned int threadsToDispatch = config.numThreads - 1;

	LevelPackage levelInfo;
	levelInfo.currLevel = level;
	levelInfo.inceptionLevelLOL = 0;
	levelInfo.threadIndex = 0;
	levelInfo.useRAM = usedRAM[0];
	levelInfo.coreIndex = 0;

	//Do one prediction for them all
	bool prediction = PredictHardDiskOrRAMProcessing(levelInfo, stats.GetLevelRecording(1), config.files[f]->fileStringSize);

	vector<vector<string>> fileList = prevFileNameList;
	PrepDataFirstLevel(prediction, fileList, prevPListArray);

	//use that one prediction
	if(usedRAM[0])
	{
		vector<vector<PListType>> balancedTruncList = ProcessThreadsWorkLoadRAMFirstLevel(threadsToDispatch, prevPListArray);
		vector<unsigned int> localWorkingThreads;
		for(unsigned int i = 0; i < balancedTruncList.size(); i++)
		{
			activeThreads[i] = true;
			localWorkingThreads.push_back(i);
		}

		countMutex->lock();
		for (unsigned int i = 0; i < localWorkingThreads.size(); i++)
		{
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
		vector<vector<string>> balancedTruncList = ProcessThreadsWorkLoadHD(threadsToDispatch, levelInfo, files);
		vector<unsigned int> localWorkingThreads;
		for(unsigned int i = 0; i < balancedTruncList.size(); i++)
		{
			activeThreads[i] = true;
			localWorkingThreads.push_back(i);
		}
		countMutex->lock();
		for (unsigned int i = 0; i < localWorkingThreads.size(); i++)
		{
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

bool Forest::NextLevelTreeSearchRecursion(vector<vector<PListType>*>* prevLocalPListArray, vector<vector<PListType>*>* globalLocalPListArray, vector<string>& fileList, LevelPackage& levelInfo, PListType patternCount, bool processingRAM)
{
	bool prediction = processingRAM ? true : false;
	PrepData(prediction, levelInfo, fileList, prevLocalPListArray, globalLocalPListArray);
	return prediction;
}

void Forest::WaitForThreads(vector<unsigned int> localWorkingThreads, vector<future<void>> *localThreadPool, bool recursive, unsigned int level)
{
	//try
	//{
		PListType threadsFinished = 0;
		StopWatch oneSecondTimer;
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
	//}
	//catch(std::exception& e)
	//{
	//	cout << "Wait for threads exception: " << e.what() << endl;
	//	//raise (SIGABRT);
	//}
}

vector<vector<string>> Forest::ProcessThreadsWorkLoadHD(unsigned int threadsToDispatch, LevelPackage levelInfo, vector<string> prevFileNames)
{

	vector<vector<string>> newFileList;
	//chunk files
	vector<PListArchive*> threadFiles;
	stringstream threadFilesNames;
	unsigned int threadNumber = 0;
	newFileList.resize(threadsToDispatch);
	if(prevFileNames.size() >= threadsToDispatch)
	{

		//Logger::WriteLog("Not distributing files!");
		for(int a = 0; a < prevFileNames.size(); a++)
		{

			newFileList[threadNumber].push_back(prevFileNames[a]);

			/*stringstream stringbuilder;
			stringbuilder << "Original file being non distributed : " << newFileList[threadNumber][newFileList[threadNumber].size() - 1] << endl;
			Logger::WriteLog(stringbuilder.str());*/

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

		//Logger::WriteLog("Distributing files!\n");

		for(unsigned int a = 0; a < threadsToDispatch; a++)
		{
			threadFilesNames.str("");
			threadFilesNames << "PListChunks" << chunkFactorio->GenerateUniqueID();

			threadFiles.push_back(new PListArchive(threadFilesNames.str(), true));
			newFileList[a].push_back(threadFilesNames.str());

			/*stringstream stringbuilder;
			stringbuilder << "New file being distributed : " << threadFilesNames.str() << " at level " << levelInfo.currLevel << " at inception " << levelInfo.inceptionLevelLOL << endl;
			Logger::WriteLog(stringbuilder.str());*/

		}

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
			chunkFactorio->DeleteArchive(prevFileNames[prevChunkCount], ARCHIVE_FOLDER);

		}

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
				balancedSizeList[smallestIndex] += (*patterns)[i]->size();
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
				patternTotals[i] += (*patterns)[i*threadsToDispatch + z]->size();
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
			patternTotals.push_back((*patterns)[i]->size());
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

	double threadMemoryConsumptionInMB = MemoryUtils::GetProgramMemoryConsumption();

	int currentFile = 0;
	int prevCurrentFile = currentFile;
	bool memoryOverflow = false;
	PListType interimCount = 0;
	unsigned int threadNumber = 0;
	unsigned int threadsToDispatch = config.numThreads - 1;

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

			//Our job is to trust whoever made the previous chunk made it within the acceptable margin of error so we compensate by saying take up to double the size if the previous
			//chunk went a little over the allocation constraint

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

			if(!memoryOverflow)
			{
				archiveCollection[a]->GetPListArchiveMMAP(packedPListArray);
				packedPListSize = packedPListArray.size();

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
				PListType sizeOfPackedPList = std::stoll (copyString,&sz);
				stringBuffer = stringBufferFile->GetPatterns(currLevel, packedPListSize);

			}
			else
			{
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
					PListType sizeOfPackedPList = std::stoll (copyString,&sz);
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
								earlyWriteIndex = fileNamesBackup.size() - prevCurrentFile;
								break;
							}
						}
						stringBufferLocal->clear();
						delete stringBufferLocal;
					}

				}

				PListType patternsToDumpCount = totalPatterns - patternsThatCantBeDumped.size();
				stringstream sizeDifference;
				patternsThatCantBeDumped.unique();

				//CODE FOR NO MATCHES SO WE DUMP EVERYTHING
				if(patternsThatCantBeDumped.size() == 0)
				{ 
					//ADDED CODE

					//Logger::WriteLog("Purging the entire map mwuahahah!\n");

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
								threadNumber %= threadsToDispatch;
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
							stats.SetMostCommonPattern(currLevel, iterator->second->size(), (*iterator->second)[0] - currLevel);
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
							chunkFactorio->DeleteChunk(newFileNames[newFileNames.size() - 1], ARCHIVE_FOLDER);
							newFileNames.pop_back();
						}
					}
					finalMetaDataMap.clear();

					//END OF ADDED CODE
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
				PListType sizeOfPackedPList = std::stoll (copyString,&sz);
				stringBufferFile = new PListArchive(fileName);
				stringBuffer = stringBufferFile->GetPatterns(currLevel, sizeOfPackedPList);
				packedPListSize = sizeOfPackedPList;

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

				chunkFactorio->DeleteChunk(fileNameForLater, ARCHIVE_FOLDER);

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

		//thread files
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
					threadNumber %= threadsToDispatch;
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
				stats.SetMostCommonPattern(currLevel, iterator->second->size(), (*iterator->second)[0] - currLevel);
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
				chunkFactorio->DeleteChunk(newFileNames[newFileNames.size() - 1], ARCHIVE_FOLDER);
				newFileNames.pop_back();
			}
		}

		for(int a = prevCurrentFile; a < currentFile; a++)
		{
			chunkFactorio->DeleteChunk(fileNamesBackup[a], ARCHIVE_FOLDER);
		}
		prevCurrentFile = currentFile;
	}

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
	int currentFile = 0;
	int prevCurrentFile = currentFile;
	bool memoryOverflow = false;
	PListType interimCount = 0;
	unsigned int threadNumber = 0;
	unsigned int threadsToDispatch = config.numThreads - 1;
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
	for(int a = 0; a < currPatternCount; a++)
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
		threadNumber %= threadsToDispatch;
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
			packedPListSize = packedPListArray.size();

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
			PListType sizeOfPackedPList = std::stoll (copyString,&sz);
			stringBuffer = stringBufferFile->GetPatterns(currLevel, packedPListSize);


			PListType countAdded = 0;
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
							patternCounts[pattern].first += packedPListArray[partialLists]->size();
						}
						else
						{
							patternCounts[pattern].first = packedPListArray[partialLists]->size();
							patternCounts[pattern].second = (*(packedPListArray[partialLists]))[0];
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

			chunkFactorio->DeleteChunk(fileNameForLater, ARCHIVE_FOLDER);

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


		for(int a = 0; a < currPatternCount; a++)
		{
			stringstream buff;
			buff << (char)a;
			currChunkFiles[buff.str()]->WriteArchiveMapMMAP(vector<PListType>(), "", true);
			bool empty = true;
			PListType patterCount = (currChunkFiles[buff.str()]->prevMappingIndex/sizeof(PListType)) - sizeof(PListType);
			if(currChunkFiles[buff.str()]->mappingIndex > (2*sizeof(PListType)))
			{
				empty = false;
				interimCount++;
				stats.SetMostCommonPattern(currLevel, patternCounts[buff.str()].first, patternCounts[buff.str()].second - currLevel);
				
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
				chunkFactorio->DeleteChunk(fileToDelete, ARCHIVE_FOLDER);
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
	//Divide between file load and previous level pLists and leave some for new lists 
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
			for(PListType prevChunkCount = 0; prevChunkCount < fileList.size(); prevChunkCount++)
			{
				PListArchive archive(fileList[prevChunkCount]);
				while(!archive.IsEndOfFile())
				{

					vector<vector<PListType>*> packedPListArray;
					archive.GetPListArchiveMMAP(packedPListArray, memDivisor/1000000.0f); 

					if(packedPListArray.size() > 0)
					{
						PListType packedListSize = packedPListArray.size();
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


							vector<TreeHD> leaflet;//(packedListSize);

							for(PListType i = 0; i < packedListSize; i++)
							{
								vector<PListType>* pList = packedPListArray[i];
								PListType pListLength = packedPListArray[i]->size();
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
														indexForString = saveOffPreviousStringData.size() - relativeIndex;
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
									if(/*!justPassedMemorySize*/leaflet.size() > 0 && leaflet[0].leaves.size() > 0)
									{
										justPassedMemorySize = true;
										stringstream stringBuilder;
										stringBuilder << chunkFactorio->GenerateUniqueID();
										fileNamesToReOpen.push_back(chunkFactorio->CreateChunkFile(stringBuilder.str(), leaflet, levelInfo));
									}
									else
									{

										//If memory is unavailable sleep for one second
										//std::this_thread::sleep_for (std::chrono::seconds(1));
									}
									i--;
								}
							}

							if(leaflet.size() > 0 && leaflet[0].leaves.size() > 0)
							{
								stringstream stringBuilder;
								PListType newID = chunkFactorio->GenerateUniqueID();
								stringBuilder << newID;
								fileNamesToReOpen.push_back(chunkFactorio->CreateChunkFile(stringBuilder.str(), leaflet, levelInfo));
							}

						}
					
						for(PListType p = 0; p < packedPListArray.size(); p++)
						{
							delete packedPListArray[p];
						}
					}
				}
				archive.CloseArchiveMMAP();
			}

			newPatternCount += ProcessChunksAndGenerate(fileNamesToReOpen, newFileNames, memDivisor, threadNum, currLevel, levelInfo.coreIndex);
			
			if(!config.history)
			{
				chunkFactorio->DeleteArchives(fileList, ARCHIVE_FOLDER);
			}

			fileList.clear();
			for(int i = 0; i < newFileNames.size(); i++)
			{
				fileList.push_back(newFileNames[i]);
			}
			fileNamesToReOpen.clear();
			newFileNames.clear();

			countMutex->lock();
			stats.SetEradicationsPerLevel(currLevel, stats.GetEradicationsPerLevel(currLevel) + removedPatterns);
			stats.SetEradicatedPatterns(stats.GetEradicatedPatterns() + removedPatterns);
			countMutex->unlock();

			if(newPatternCount > 0 && levelInfo.currLevel < config.maximum)
			{
				levelInfo.currLevel++;
				//Have to add prediction here 
				bool prediction = PredictHardDiskOrRAMProcessing(levelInfo, newPatternCount, (config.files[f]->fileStringSize - stats.GetEradicatedPatterns())/(config.numThreads - 1));
				if(!prediction)
				{
					morePatternsToFind = false;
					break;
				}
				else
				{
					
					morePatternsToFind = true;
					DispatchNewThreadsHD(newPatternCount, morePatternsToFind, fileList, levelInfo, isThreadDefuncted);
				}
			}
			else
			{
				chunkFactorio->DeleteArchives(fileList, ARCHIVE_FOLDER);
				morePatternsToFind = false;
			}
		}
	}
	catch(exception e)
	{
		cout << e.what() << endl;
		MemoryUtils::print_trace();
	}

	return newPatternCount;
}

bool Forest::DispatchNewThreadsRAM(PListType newPatternCount, PListType& morePatternsToFind, vector<PListType> &linearList, vector<PListType> &pListLengths, LevelPackage levelInfo, bool& isThreadDefuncted)
{
	bool dispatchedNewThreads = false;
	bool alreadyUnlocked = false;
	countMutex->lock();

	int threadsToDispatch = config.numThreads - 1;
	int unusedCores = (threadsToDispatch - (threadsDispatched - threadsDefuncted)) + 1;
	
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
				if(threadsToTest + localWorkingThreads.size() <= threadsToDispatch)
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

	int threadsToDispatch = config.numThreads - 1;
	int unusedCores = (threadsToDispatch - (threadsDispatched - threadsDefuncted)) + 1;
	if(newPatternCount < unusedCores && unusedCores > 1)
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
				if(threadsToTest + localWorkingThreads.size() <= threadsToDispatch)
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

						vector<PListType> temp;
#if defined(_WIN64) || defined(_WIN32)
						localThreadPool->push_back(std::async(std::launch::async, &Forest::ThreadedLevelTreeSearchRecursionList, this, prevPListArray, temp, balancedTruncList[i], levelInfoRecursion));
#else
						localThreadPool->push_back(std::async(std::launch::async, &Forest::ThreadedLevelTreeSearchRecursionList, this, prevPListArray, temp, balancedTruncList[i], levelInfoRecursion));
#endif
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
	int threadsToDispatch = config.numThreads - 1;
	PListType totalTallyRemovedPatterns = 0;
	PListType newPatterns = 0;
	string globalStringConstruct;
	vector<PListType> pListTracker;
	PListType fileSize = config.files[f]->fileStringSize;
	PListType totalCount = 0;
	if(levelInfo.currLevel == 2)
	{
		int threadCountage = 0;

		for (PListType i = 0; i < prevLocalPListArray->size(); i++)
		{
			PListType pListLength = (*prevLocalPListArray)[i]->size();
			if(pListLength > 0)
			{
				totalCount += pListLength;
			}
		}
		prevLinearList.reserve(totalCount);

		for (PListType i = 0; i < prevLocalPListArray->size(); i++)
		{
			PListType pListLength = (*prevLocalPListArray)[i]->size();
			if(pListLength > 0)
			{
				threadCountage += pListLength;
				prevLinearList.insert(prevLinearList.end(), (*prevLocalPListArray)[i]->begin(), (*prevLocalPListArray)[i]->end());
				delete  (*prevLocalPListArray)[i];
			}
			else
			{
				delete (*prevLocalPListArray)[i];
			}

			if(!firstLevelProcessedHD)
			{
				if(i % threadsToDispatch == (threadsToDispatch - 1) && threadCountage > 1)
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
	else
	{

		for (PListType i = 0; i < prevLocalPListArray->size(); i++)
		{
			PListType pListLength = (*prevLocalPListArray)[i]->size();
			if(pListLength > 0)
			{
				totalCount += pListLength;
			}
		}

		prevLinearList.reserve(totalCount);

		for (PListType i = 0; i < prevLocalPListArray->size(); i++)
		{
			PListType pListLength = (*prevLocalPListArray)[i]->size();
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

		if(levelInfo.currLevel == 2)
		{
			PListType linearListSize = prevLinearList.size();
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
			PListType linearListSize = prevLinearList.size();
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

		globalStringConstruct.resize(stringIndexer);
		globalStringConstruct.shrink_to_fit();
		stringIndexer = 0;
		vector<PListType> newPList[256];

		if(levelInfo.currLevel == 2)
		{
			PListType prevPListSize = prevLinearList.size();
			PListType indexes[256] = {0};
			PListType indexesToPush[256] = {0};
			//Banking off very random patterns
			PListType firstPatternIndex[256] = {0};
			PListType prevSize = 0;
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
					uint_fast8_t indexIntoFile = (uint8_t)globalStringConstruct[stringIndexer++];
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
					for (PListType k = 0; k < listLength; k++)
					{
						int insert = indexes[indexesToPush[k]];
						if (insert >= config.minOccurrence)
						{
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
								pListLengths.push_back(newPList[indexesToPush[k]].size());
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
		else
		{
			PListType prevPListSize = prevLinearList.size();
			PListType indexes[256] = {0};
			PListType indexesToPush[256] = {0};
			//Banking off very random patterns
			PListType firstPatternIndex[256] = {0};
			PListType prevSize = 0;
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
					uint_fast8_t indexIntoFile = (uint8_t)globalStringConstruct[stringIndexer++];
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
					for (PListType k = 0; k < listLength; k++)
					{
						int insert = indexes[indexesToPush[k]];
						if (insert >= config.minOccurrence)
						{
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
								pListLengths.push_back(newPList[indexesToPush[k]].size());
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

		countMutex->lock();

		stats.SetEradicationsPerLevel(levelInfo.currLevel, stats.GetEradicationsPerLevel(levelInfo.currLevel) + totalTallyRemovedPatterns);
		stats.SetEradicatedPatterns(stats.GetEradicatedPatterns() + totalTallyRemovedPatterns);
		stats.SetLevelRecording(levelInfo.currLevel, stats.GetLevelRecording(levelInfo.currLevel) + pListLengths.size());

		PListType tempMostCommonPatternCount = stats.GetMostCommonPatternCount(levelInfo.currLevel);
		PListType tempMostCommonPatternIndex = 0;

		stats.SetCurrentLevel(levelInfo.threadIndex, levelInfo.currLevel + 1);
		
		countMutex->unlock();

		PListType countage = 0;
		PListType indexOfList = 0;
		bool chosen = false;
		PListType unalteredCount = 0;
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
					tempMostCommonPatternIndex = linearList[countage] - levelInfo.currLevel;
					indexOfList = countage;
					chosen = true;
				}
			}
			countage += pListLengths[i];
		}

		

		if(chosen/* && tempMostCommonPatternCount > stats.GetMostCommonPatternCount(levelInfo.currLevel)*/)
		{
			countMutex->lock();
			stats.SetMostCommonPattern(levelInfo.currLevel, tempMostCommonPatternCount, tempMostCommonPatternIndex);
			countMutex->unlock();

			//Monitor number of patterns that do not overlap ie coverage
			PListType index = indexOfList;
			countMutex->lock();
			PListType count = unalteredCount;
			countMutex->unlock();
			PListType totalTally = 0;
			double percentage = 0;
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
				percentage = ((double)(totalCoverage)) / ((double)(config.files[f]->fileStringSize));
			}

			countMutex->lock();
			stats.SetCoverage(levelInfo.currLevel, percentage);
			countMutex->unlock();

			if(totalTally != count)
			{
				//cout << "Number of overlapping patterns: " << count - totalTally << endl;
			}
		}

		levelInfo.currLevel++;


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
			//Have to add prediction here 
			bool prediction = PredictHardDiskOrRAMProcessing(levelInfo, pListLengths.size(), linearList.size());
			if(prediction)
			{

				PListType indexing = 0;
				prevLocalPListArray->clear();
				for(int piss = 0; piss < pListLengths.size(); piss++)
				{
					prevLocalPListArray->push_back(new vector<PListType>(linearList.begin() + indexing, linearList.begin() + indexing + pListLengths[piss]));
					indexing += pListLengths[piss];
				}

				continueSearching = pListLengths.size();

				linearList.clear();
				linearList.reserve(0);
				pListLengths.clear();
				pListLengths.reserve(0);
				break;
			}
			else
			{
				continueSearching = pListLengths.size();
				
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
	return continueSearching;
}

void Forest::ThreadedLevelTreeSearchRecursionList(vector<vector<PListType>*>* patterns, vector<PListType> patternIndexList, vector<string> fileList, LevelPackage levelInfo)
{
	PListType numPatternsToSearch = patternIndexList.size();
	bool isThreadDefuncted = false;
	//cout << "Threads dispatched: " << threadsDispatched << " Threads deported: " << threadsDefuncted << " Threads running: " << threadsDispatched - threadsDefuncted << endl;

	int tempCurrentLevel = levelInfo.currLevel;
	int threadsToDispatch = config.numThreads - 1;

	if(threadsDispatched - threadsDefuncted > threadsToDispatch)
	{
		cout << "WENT OVER THREADS ALLOCATION SIZE!" << endl;
	}

	vector<vector<PListType>*>* prevLocalPListArray = new vector<vector<PListType>*>();
	vector<vector<PListType>*>*	globalLocalPListArray = new vector<vector<PListType>*>();

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
		
		if(levelInfo.currLevel != 2)
		{
			if(!config.usingPureHD && !config.usingPureRAM)
			{
				useRAMBRO = !NextLevelTreeSearchRecursion(prevLocalPListArray, globalLocalPListArray, fileList, levelInfo, continueSearching, processingRAM);
			}
		}
		else
		{
			useRAMBRO = levelInfo.useRAM;
		}

		if(useRAMBRO)
		{
			continueSearching = ProcessRAM(prevLocalPListArray, globalLocalPListArray, levelInfo, isThreadDefuncted);
			processingRAM = true;
		}
		else
		{
			continueSearching = ProcessHD(levelInfo, fileList, isThreadDefuncted);
			processingRAM = false;
		}
	}

	if(prevLocalPListArray != NULL)
	{
		delete prevLocalPListArray;
	}

	if(globalLocalPListArray != NULL)
	{
		delete globalLocalPListArray;
	}

	countMutex->lock();
	if(!isThreadDefuncted)
	{
		threadsDefuncted++;
	}
	countMutex->unlock();

	return;
}

void Forest::PlantTreeSeedThreadHD(PListType positionInFile, PListType startPatternIndex, PListType numPatternsToSearch, unsigned int threadNum)
{
	LevelPackage levelInfo;
	levelInfo.currLevel = 1;
	levelInfo.coreIndex = threadNum;
	levelInfo.threadIndex = threadNum;

	int threadsToDispatch = config.numThreads - 1;
	PListType earlyApproximation = config.files[f]->fileString.size()/(256*(config.numThreads - 1));
	vector<vector<PListType>*> leaves(256);
	for(int i = 0; i < 256; i++)
	{
		leaves[i] = new vector<PListType>();
		leaves[i]->reserve(earlyApproximation);
	}

	PListType counting = 0;
	PListType memDivisorInMB = (PListType)(memoryPerThread/3.0f);
	//used primarily for just storage containment
	for (PListType i = startPatternIndex; i < numPatternsToSearch + startPatternIndex; i++)
	{
		int temp = i + positionInFile + 1;
		uint8_t tempIndex = (uint8_t)config.files[f]->fileString[i];
		if(config.patternToSearchFor.size() == 0 || config.files[f]->fileString[i] == config.patternToSearchFor[0])
		{
			leaves[tempIndex]->push_back(temp);
			counting++;
		}
		if(overMemoryCount && counting >= PListArchive::hdSectorSize)
		{
			stringstream stringBuilder;
			PListType newID = chunkFactorio->GenerateUniqueID();
			stringBuilder << newID;
			newFileNameList[threadNum].push_back(chunkFactorio->CreateChunkFile(stringBuilder.str(), leaves, levelInfo));
			for(int i = 0; i < 256; i++)
			{
				leaves[i] = new vector<PListType>();
				leaves[i]->reserve(earlyApproximation);
			}
		}
	}

	stringstream stringBuilder;
	PListType newID = chunkFactorio->GenerateUniqueID();
	stringBuilder << newID;
	newFileNameList[threadNum].push_back(chunkFactorio->CreateChunkFile(stringBuilder.str(), leaves, levelInfo));
	
	return;
}

void Forest::PlantTreeSeedThreadRAM(PListType positionInFile, PListType startPatternIndex, PListType numPatternsToSearch, PListType threadIndex)
{
	int threadsToDispatch = config.numThreads - 1;
	PListType earlyApproximation = config.files[f]->fileString.size()/(256*(config.numThreads - 1));
	vector<PListType>* leaves[256];
	for(int i = 0; i < 256; i++)
	{
		leaves[i] = new vector<PListType>();
		leaves[i]->reserve(earlyApproximation);
	}
	PListType endPatternIndex = numPatternsToSearch + startPatternIndex;

	for (PListType i = startPatternIndex; i < endPatternIndex; i++)
	{
		int temp = i + positionInFile + 1;
		uint8_t tempIndex = (uint8_t)config.files[f]->fileString[i];
		if(config.patternToSearchFor.size() == 0 || config.files[f]->fileString[i] == config.patternToSearchFor[0])
		{
			leaves[tempIndex]->push_back(temp);
		}
	}

	for(int i = 0; i < 256; i++)
	{
		(*prevPListArray)[threadIndex + i*threadsToDispatch] = leaves[i];
	}
	stats.SetCurrentLevel(threadIndex, 2);
}

