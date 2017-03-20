#include "Forest.h"
#include "MemoryUtils.h"
#include <locale>
#include <list>
#include <algorithm>
#include <signal.h>

bool Forest::outlierScans = false;
bool Forest::overMemoryCount = false;
Forest::Forest(int argc, char **argv)
{

#if defined(_WIN64) || defined(_WIN32)
	system("del ..\\..\\Log\\PList*.txt");
#elif defined(__linux__)
	system("rm -r ../Log/PList*");
#endif

#if defined(_WIN64) || defined(_WIN32)
	//Hard code page size to 2 MB for windows
	PListArchive::hdSectorSize = 2097152;//4096;
#elif defined(__linux__)
	PListArchive::hdSectorSize = sysconf (_SC_PAGESIZE);
#endif

	PListArchive::totalLoops = PListArchive::hdSectorSize/sizeof(PListType);
	PListArchive::writeSize = PListArchive::hdSectorSize/8;

	f = 0;
	writingFlag = false;
	fileID = 0;
	fileIDMutex = new mutex();

	threadsDispatched = 0;
	threadsDefuncted = 0;
	mostMemoryOverflow = 0;
	currMemoryOverflow = 0;


	displayEachLevelSearch = false;
	findBestThreadNumber = false;
	globalLevel = 1;
	usingMemoryBandwidth = false;
	memoryBandwidthMB = 0;
	levelToOutput = 0;
	history = 0;
	eradicatedPatterns = 0;
	usingPureRAM = false;
	usingPureHD = false;
	//Default pattern occurence size to 2
	minOccurrence = 2;
	firstLevelProcessedHD = false;

	CommandLineParser(argc, argv);

	MemoryUsageAtInception = MemoryUtils::GetProgramMemoryConsumption();
	MemoryUsedPriorToThread = MemoryUtils::GetProgramMemoryConsumption();
	overMemoryCount = false;
	processingFinished = false;
	processingMSYNCFinished = false;

	countMutex = new mutex();
	threadPool = new vector<future<void>>();
	threadPlantSeedPoolHD = NULL;
	threadPlantSeedPoolRAM = NULL;
	prevPListArray = new vector<vector<PListType>*>();
	globalPListArray = new vector<vector<PListType>*>();

	//Initialize all possible values for the first list to NULL
	for(int i = 0; i < 256; i++)
	{
		gatedMutexes.push_back(new mutex());
	}

	
	//main thread is a hardware thread so dispatch threads requested minus 1
	int threadsToDispatch = 0;
	if(findBestThreadNumber)
	{
		numThreads = 2;
	}
	threadsToDispatch = numThreads - 1;


	memoryCeiling = (PListType)MemoryUtils::GetAvailableRAMMB() - 1000;

	//If memory bandwidth not an input
	if(!usingMemoryBandwidth)
	{

		//Leave 1 GB to spare for operating system in case our calculations suck
		memoryBandwidthMB = memoryCeiling - 1000;
	}

	thread *memoryQueryThread = NULL;
	thread *msyncThread = NULL;
	
	double threadMemoryConsumptionInMB = MemoryUtils::GetProgramMemoryConsumption();
	stringstream crappy;
	crappy << "Errant memory after processing level " << threadMemoryConsumptionInMB - MemoryUsageAtInception << " in MB!\n";
	Logger::WriteLog(crappy.str());

	vector<map<int, double>> threadMap;

	
	for(f = 0; f < files.size(); f++)
	{
		PListType earlyApproximation = files[f]->fileString.size()/(256);

		const char * c = files[f]->fileName.c_str();
	
		// Open the file for the shortest time possible.
		files[f]->copyBuffer = new ifstream(c, ios::binary);

		if (!files[f]->copyBuffer->is_open()) 
		{
			//Close file handle once and for all
			files[f]->copyBuffer->clear();
			files[f]->fileString.clear();
			files[f]->fileString.reserve(0);

			delete files[f];
			continue;
		}

		if(files[f]->fileString.size() == 0 && usingPureRAM)
		{
			//new way to read in file
			files[f]->fileString.clear();
			files[f]->fileString.resize(files[f]->fileStringSize);
			files[f]->copyBuffer->read( &files[f]->fileString[0], files[f]->fileString.size());
		}
		
		for(unsigned int threadIteration = 0; threadIteration <= testIterations; threadIteration = threadsToDispatch)
		{

			stringstream loggingIt;
			loggingIt.str("");
			std::string::size_type i = files[f]->fileName.find(DATA_FOLDER);
			string nameage = files[f]->fileName;
			if (i != std::string::npos)
				nameage.erase(i, sizeof(DATA_FOLDER)-1);

			loggingIt << "\n" << Logger::GetTime() << " File processing starting for: " << nameage << endl << endl;
			Logger::WriteLog(loggingIt.str());
			cout << loggingIt.str();

			fileID = 0;
			initTime.Start();

			prevFileNameList.clear();
			newFileNameList.clear();
			prevFileNameList.resize(numThreads - 1);
			newFileNameList.resize(numThreads - 1);

			fileChunks.clear();
			fileChunks.reserve(0);

			chunkIndexToFileChunk.clear();


			//if(!usingPureRAM)
			//{
				//Kick off thread that processes how much memory the program uses at a certain interval
				memoryQueryThread = new thread(&Forest::MemoryQuery, this);
			//}

			if(!usingPureRAM)
			{
				//Kick off thread that processes how much memory the program uses at a certain interval
				msyncThread = new thread(&Forest::MonitorMSYNCThreads, this);
			}

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

			currentLevelVector.resize(threadsToDispatch);
			activeThreads.resize(threadsToDispatch);
			for(int i = 0; i < threadsToDispatch; i++)
			{
				currentLevelVector[i] = 0;
				activeThreads[i] = false;
			}

			memoryPerThread = memoryBandwidthMB/threadsToDispatch;
			cout << "Memory that can be used per thread: " << memoryPerThread << " MB." << endl;

			
			PListType overallFilePosition = 0;

			//make this value 1 so calculations work correctly then reset
			LevelPackage levelInfo;
			levelInfo.currLevel = 1;
			levelInfo.inceptionLevelLOL = 0;
			levelInfo.threadIndex = 0;
			levelInfo.useRAM = usedRAM[0];
			levelInfo.coreIndex = 0;
			bool prediction = PredictHardDiskOrRAMProcessing(levelInfo, 1);
			firstLevelProcessedHD = prediction;
			for(int i = 0; i < threadsToDispatch; i++)
			{
				usedRAM[i] = !prediction;
				currentLevelVector[i] = 1;
			}


			PListType fileReadSize = (PListType)((memoryPerThread*1000000)/3.0f);
			PListType fileIterations = files[f]->fileStringSize/fileReadSize;
			if(files[f]->fileStringSize%fileReadSize != 0)
			{
				fileIterations++;
			}

			//If using pure ram or prediction say we can use ram then we don't care about memory constraints
			if(usingPureRAM || !prediction)
			{
				fileIterations = 1;
			}

			vector<string> backupFilenames;

			files[f]->copyBuffer->clear();
			files[f]->copyBuffer->seekg(0, ios::beg);

			//Only start processing time after file is read in
			StopWatch time;

			cout << "Number of threads processing file is " << threadsToDispatch << endl;

			for(int z = 0; z < fileIterations; z++)
			{
				PListType position = 0;
				PListType patternCount = 0;
				if(files[f]->fileStringSize <= fileReadSize*z + fileReadSize)
				{
					patternCount = files[f]->fileStringSize - fileReadSize*z;
				}
				else
				{
					patternCount = fileReadSize;
				}

				//load in the entire file
				if(usingPureRAM || !prediction)
				{
					patternCount = files[f]->fileStringSize;
				}

				if(!usingPureRAM)
				{
					//new way to read in file
					files[f]->fileString.clear();
					files[f]->fileString.resize(patternCount);
					files[f]->copyBuffer->read( &files[f]->fileString[0], files[f]->fileString.size());
				}

				
			
				PListType cycles = patternCount/threadsToDispatch;
				PListType lastCycle = patternCount - (cycles*threadsToDispatch);
				PListType span = cycles;

				if(prediction)
				{
					threadPlantSeedPoolHD = new vector<future<void>>();
				}
				else
				{
					threadPlantSeedPoolRAM = new vector<future<void>>();
				}


				for(int i = 0; i < threadsToDispatch; i++)
				{
					if(!(i < threadsToDispatch - 1))
					{
						span = span + lastCycle;
					}	

					if(prediction)
					{
#if defined(_WIN64) || defined(_WIN32)
						threadPlantSeedPoolHD->push_back(std::async(std::launch::async, &Forest::PlantTreeSeedThreadHD, this, overallFilePosition, position, span, i));
#else
						threadPlantSeedPoolHD->push_back(std::async(std::launch::async, &Forest::PlantTreeSeedThreadHD, this, overallFilePosition, position, span, i));
#endif
					}
					else
					{
#if defined(_WIN64) || defined(_WIN32)
						threadPlantSeedPoolRAM->push_back(std::async(std::launch::async, &Forest::PlantTreeSeedThreadRAM, this, overallFilePosition, position, span, i));
#else
						threadPlantSeedPoolRAM->push_back(std::async(std::launch::async, &Forest::PlantTreeSeedThreadRAM, this, overallFilePosition, position, span, i));
#endif
					}
					position += span;
				}

				if(prediction)
				{
					vector<string> test;
					FirstLevelHardDiskProcessing(test, z);
				}
				else
				{
					vector<unsigned int> localWorkingThreads;
					for(unsigned int i = 0; i < threadsToDispatch; i++)
					{
						localWorkingThreads.push_back(i);
					}
					WaitForThreads(localWorkingThreads, threadPlantSeedPoolRAM); 


					if(levelRecordings.size() < levelInfo.currLevel)
					{
						levelRecordings.resize(levelInfo.currLevel);
					}
					levelRecordings[0] = 256;
					if(mostCommonPatternIndex.size() < levelInfo.currLevel)
					{
						mostCommonPatternIndex.resize(levelInfo.currLevel);
						mostCommonPatternCount.resize(levelInfo.currLevel);
					}
					
					
					PListType indexOfList = 0;
					std::map<string, PListType> countMap;
					std::map<string, vector<PListType>> indexMap;
					for (PListType i = 0; i < prevPListArray->size(); i++)
					{
						if((*prevPListArray)[i] != nullptr && (*prevPListArray)[i]->size() > 0)
						{
							countMap[files[f]->fileString.substr((*(*prevPListArray)[i])[0] - (levelInfo.currLevel), levelInfo.currLevel)] += (*prevPListArray)[i]->size();
							
							if( countMap[files[f]->fileString.substr((*(*prevPListArray)[i])[0] - (levelInfo.currLevel), levelInfo.currLevel)] > mostCommonPatternCount[levelInfo.currLevel - 1])
							{
								mostCommonPatternCount[levelInfo.currLevel - 1] = countMap[files[f]->fileString.substr((*(*prevPListArray)[i])[0] - (levelInfo.currLevel), levelInfo.currLevel)];
								mostCommonPatternIndex[levelInfo.currLevel - 1] = (*(*prevPListArray)[i])[0] - (levelInfo.currLevel);
								indexOfList = i;
							}

							if((*prevPListArray)[i]->size() >= 1)
							{
								indexMap[files[f]->fileString.substr((*(*prevPListArray)[i])[0] - (levelInfo.currLevel), levelInfo.currLevel)].push_back(i);
							}
						}
					}

					levelRecordings[levelInfo.currLevel - 1] = countMap.size();


					PListType countage = 0;
					if(coverage.size() < levelInfo.currLevel)
					{
						coverage.resize(levelInfo.currLevel);
					}

					//Monitor number of patterns that do not overlap ie coverage
					PListType index = indexOfList % 256;
					PListType count = mostCommonPatternCount[levelInfo.currLevel - 1];
					PListType totalTally = 0;
					PListType prevIndex = 0;

					for(int i = 0; i < threadsToDispatch; i++)
					{
						if((*prevPListArray)[index + i]->size() > 1)
						{
							prevIndex = (*(*prevPListArray)[index + i])[0];
							totalTally++;
		
							for(PListType j = 1; j < (*prevPListArray)[index + i]->size(); j++)
							{
								PListType span = (*(*prevPListArray)[index])[j] - prevIndex;
								if(span >= levelInfo.currLevel)
								{
									PListType pIndex = (*(*prevPListArray)[index])[j] - levelInfo.currLevel;
									totalTally++;
									prevIndex = pIndex;
								}
							}
						}
					}
					//Coverage of most common pattern per level
					if(totalTally * levelInfo.currLevel > coverage[levelInfo.currLevel - 1])
					{
						coverage[levelInfo.currLevel - 1] = ((double)(totalTally * levelInfo.currLevel)) / ((double)(files[f]->fileStringSize));
					}
					if(totalTally != count)
					{
						//cout << "Number of overlapping patterns: " << count - totalTally << endl;
					}



					for(map<string, vector<PListType>>::iterator it = indexMap.begin(); it != indexMap.end(); it++)
					{
						if(it->second.size() == 1 && (*prevPListArray)[it->second[0]]->size() == 1)
						{
							(*prevPListArray)[it->second[0]]->clear();
							levelRecordings[levelInfo.currLevel - 1]--;
							eradicatedPatterns++;
						}
					}
				}
				overallFilePosition += position;

				if(prediction)
				{
					threadPlantSeedPoolHD->erase(threadPlantSeedPoolHD->begin(), threadPlantSeedPoolHD->end());
					(*threadPlantSeedPoolHD).clear();
					delete threadPlantSeedPoolHD;
				}
				else
				{
					threadPlantSeedPoolRAM->erase(threadPlantSeedPoolRAM->begin(), threadPlantSeedPoolRAM->end());
					(*threadPlantSeedPoolRAM).clear();
					delete threadPlantSeedPoolRAM;
				}

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

			//If not conserving memory dont clear out for next level processing
			if(!usingPureRAM && prediction)
			{
				//Close file handle once and for all
				//files[f]->copyBuffer->clear();
				//files[f]->fileString.clear();
				//files[f]->fileString.reserve(0);
			}
			//Read in whole file if we are processing RAM potentially
			else
			{
				////new way to read in file
				//files[f]->fileString.clear();
				//files[f]->copyBuffer->seekg( 0 );
				//files[f]->fileString.resize(files[f]->fileStringSize);
				//files[f]->copyBuffer->read( &files[f]->fileString[0], files[f]->fileString.size());
			}

			//Divide between file load and previous level pLists and leave some for new lists haha 
			PListType memDivisor = (PListType)((memoryPerThread*1000000)/3.0f);

			int currentFile = 0;
			bool memoryOverflow = false;
			vector<string> temp;
			if(prediction)
			{
				if(levelToOutput == 0 || (levelToOutput != 0 && globalLevel >= levelToOutput))
				{
					ProcessChunksAndGenerateLargeFile(backupFilenames, temp, memDivisor, 0, 1, true);
					//ProcessChunksAndGenerate(backupFilenames, temp, memDivisor, 0, 1, levelInfo.coreIndex, true);
				}
			}
			else
			{
				//nothing for now
			}

			//Logger::WriteLog("Eradicated patterns: " + std::to_string(eradicatedPatterns) + "\n");

			DisplayPatternsFound();       

			//Start searching
			if(2 <= maximum)
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
			if(Logger::verbosity)
			{
				for(int j = 0; j < levelRecordings.size() && levelRecordings[j] != 0; j++)
				{
					string pattern = files[f]->fileString.substr(mostCommonPatternIndex[j], j + 1);
					stringstream buff;
					buff << "Level " << j + 1 << " count is " << levelRecordings[j] << " with most common pattern being: \"" << pattern << "\" occured " << mostCommonPatternCount[j] <</* " and coverage was " << coverage[j] << "%" <<*/ endl;
					Logger::WriteLog(buff.str());
				}
			}

			if(files[f]->fileStringSize != files[f]->fileString.size())
			{
				files[f]->fileString.clear();
				files[f]->copyBuffer->seekg( 0 );
				files[f]->fileString.resize(files[f]->fileStringSize);
				files[f]->copyBuffer->read( &files[f]->fileString[0], files[f]->fileString.size());
			}
			//Logger::fillPatternData(files[f]->fileString, mostCommonPatternIndex);
			Logger::fileCoverageCSV(coverage);

			finalPattern[levelRecordings.size()]++;
			levelRecordings.clear();
			mostCommonPatternCount.clear();
			mostCommonPatternIndex.clear();
			currentLevelVector.clear();

			for (int i = 0; i < prevPListArray->size(); i++)
			{
				if((*prevPListArray)[i] != NULL)
				{
					delete (*prevPListArray)[i];
				}
			}
			prevPListArray->clear();

			for (int i = 0; i < globalPListArray->size(); i++)
			{
				if((*globalPListArray)[i] != NULL)
				{
					delete (*globalPListArray)[i];
				}
			}
			globalPListArray->clear();

			if(findBestThreadNumber)
			{
				numThreads = (numThreads * 2) - 1;
				threadsToDispatch = numThreads - 1;
			}
			
			//reset global level in case we are testing
			globalLevel = 1;

			crappy.str("");
			crappy << "File Size " << files[f]->fileStringSize << " and eliminated patterns " << eradicatedPatterns << "\n\n\n";
			Logger::WriteLog(crappy.str());

			//If we aren't doing a deep search in levels then there isn't a need to check that pattern finder is properly functioning..it's impossible
			if(files[f]->fileStringSize != eradicatedPatterns && maximum == -1)
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

			if(msyncThread != NULL)
			{
				processingMSYNCFinished = true;
				msyncThread->join();
				delete msyncThread;
				msyncThread = NULL;
				processingMSYNCFinished = false; 
			}
			eradicatedPatterns = 0;
		}
		
		stringstream loggingIt;
		loggingIt.str("");
		std::string::size_type i = files[f]->fileName.find(DATA_FOLDER);
		string nameage = files[f]->fileName;
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
		fileProgressStream << "File collection percentage completed: " << f*100/files.size() << "%\n";
		Logger::WriteLog(fileProgressStream.str());

		//Close file handle once and for all
		files[f]->copyBuffer->clear();
		files[f]->fileString.clear();
		files[f]->fileString.reserve(0);

		delete files[f];

		if(findBestThreadNumber)
		{
			numThreads = 2;
			threadsToDispatch = numThreads - 1;
		}

	}

	Logger::generateTimeVsFileSizeCSV(processingTimes, fileSizes);

	Logger::generateFinalPatternVsCount(finalPattern);

	if(findBestThreadNumber)
	{
		Logger::generateThreadsVsThroughput(threadMap);
	}

	for(int i = 0; i < 256; i++)
	{
		delete gatedMutexes[i];
	}

	delete threadPool;

	delete countMutex;
	delete prevPListArray;
	delete globalPListArray;
	delete fileIDMutex;

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

void Forest::MonitorMSYNCThreads()
{
	int prevIndex = 0;
	int currIndex = 0;
	while(!processingMSYNCFinished || currIndex != PListArchive::threadKillList.size())
	{
		this_thread::sleep_for(std::chrono::milliseconds(100));
		prevIndex = currIndex;
		//PListArchive::syncLock.lock();
		const size_t listCount = PListArchive::threadKillList.size();
		//PListArchive::syncLock.unlock();
		for(int i = prevIndex; i < listCount; i++)
		{
			if(PListArchive::threadKillList[i] != NULL)
			{
				PListArchive::threadKillList[i]->join();
				currIndex++;
			}
		}
		//maintenance
		for(int i = prevIndex; i < currIndex; i++)
		{
			if(PListArchive::threadKillList[i] != NULL)
			{
				delete PListArchive::threadKillList[i];
				PListArchive::threadKillList[i] = NULL;
			}
		}
	}
	const size_t listCount = PListArchive::threadKillList.size();
	for(int i = prevIndex; i < listCount; i++)
	{
		if(PListArchive::threadKillList[i] != NULL)
		{
			PListArchive::threadKillList[i]->join();
			currIndex++;
		}
	}
	//maintenance
	for(int i = prevIndex; i < currIndex; i++)
	{
		if(PListArchive::threadKillList[i] != NULL)
		{
			delete PListArchive::threadKillList[i];
			PListArchive::threadKillList[i] = NULL;
		}
	}
	PListArchive::threadKillList.resize(0);
	PListArchive::threadKillList.clear();
}

void Forest::MemoryQuery()
{
	StopWatch swTimer;
	stringstream loggingIt;
	swTimer.Start();
	while(!processingFinished)
	{
		this_thread::sleep_for(std::chrono::milliseconds(50));
		double memoryOverflow = 0;
		overMemoryCount = MemoryUtils::IsOverMemoryCount(MemoryUsedPriorToThread, (double)memoryBandwidthMB, memoryOverflow);
		currMemoryOverflow = memoryOverflow;
		if(mostMemoryOverflow < memoryOverflow)
		{
			mostMemoryOverflow = memoryOverflow;
		}
		//Abort mission and did not exit gracefully ie dump this shit cause we will be pageing soon
		if(currMemoryOverflow + memoryBandwidthMB > memoryCeiling)
		{
			Logger::WriteLog("Have to bail because you are using too much memory for your system!");
			exit(0);
		}

		filesToBeRemovedLock.lock();
		if(filesToBeRemoved.size() > 0)
		{
			while(filesToBeRemoved.front() != filesToBeRemoved.back())
			{
				remove(filesToBeRemoved.front().c_str());
				filesToBeRemoved.pop();
			}
		}
		filesToBeRemovedLock.unlock();
		

		if(swTimer.GetTime() > 10000.0f)
		{
			loggingIt.str("");
			swTimer.Start();
			loggingIt << "Thread level status...\n";
			for(int j = 0; j < currentLevelVector.size(); j++)
			{
				loggingIt << "Thread " << j << " is at level: " << currentLevelVector[j] << endl;
			}
			cout << loggingIt.str();
			Logger::WriteLog(loggingIt.str());
			loggingIt.str("");
			loggingIt << "Percentage of file processed is: " << (((double)eradicatedPatterns)/((double)files[f]->fileStringSize))*100.0f << "%\n";
			cout << loggingIt.str();
			Logger::WriteLog(loggingIt.str());
			loggingIt.str("");
			loggingIt << "Percentage of cpu usage: " << MemoryUtils::CPULoad() << "%\n";
			cout << loggingIt.str();
			Logger::WriteLog(loggingIt.str());
			//cout << "Memory Maps in service: " << PListArchive::mappedList.size() << endl;
			initTime.DisplayNow();
		}
	}
}

void Forest::DisplayPatternsFound()
{
	//if(levelToOutput != 0 && globalLevel >= levelToOutput)
	//{
	//	//Writing to log file write to txt file and outputs to command prompt
	//	stringstream buff;
	//	buff << "Number of patterns found at level " << globalLevel << ": " << levelCountPatterns << endl;
	//	Logger::WriteLog(buff.str());
	//	cout << buff.str();
	//}
	//else
	//{
	//	//Writing to log file write to txt file and outputs to command prompt
	//	stringstream buff;
	//	buff << "Finished processing level " << globalLevel << " but did not calculate patterns generated" << endl;
	//	Logger::WriteLog(buff.str());
	//	cout << buff.str();
	//}
}

void Forest::DisplayHelpMessage()
{
	bool isFile;
	FileReader tempHelpFile(READMEPATH, isFile, true);
	tempHelpFile.LoadFile();
	cout << tempHelpFile.fileString << endl;
}

void Forest::FirstLevelHardDiskProcessing(vector<string>& backupFilenames, unsigned int z)
{
	unsigned int threadsToDispatch = numThreads - 1;
	int threadsFinished = 0;
	while(threadsFinished != threadsToDispatch)
	{
		for(unsigned int k = 0; k < threadsToDispatch; k++)
		{
			if(threadPlantSeedPoolHD != NULL)
			{
				(*threadPlantSeedPoolHD)[k].get();
				threadsFinished++;
			}
		}
	}
	threadPlantSeedPoolHD->erase(threadPlantSeedPoolHD->begin(), threadPlantSeedPoolHD->end());
	(*threadPlantSeedPoolHD).clear();
}

void Forest::FindFiles(string directory)
{
	bool isFile = false;
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

				if(!fileName.empty() && fileName != "." && fileName !=  ".." && fileName.find(".") != std::string::npos && fileName.find(".ini") == std::string::npos)
				{
					string name = string(ent->d_name);
					Logger::WriteLog(name + "\n");
					//cout << name << endl;
					string tempName = directory;
					tempName.append(ent->d_name);
					FileReader* file = new FileReader(tempName, isFile);
					if(isFile)
					{
						files.push_back(file);
						fileSizes.push_back(files.back()->fileStringSize);
					}
					else //This is probably a directory then
					{
						FindFiles(directory + fileName + "/");
					}
				}
				else if(fileName != "." && fileName !=  ".." && fileName.find(".ini") == std::string::npos)
				{
					FindFiles(directory + fileName + "/");
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
		return;
	if (!(entry = readdir(dir)))
		return;
	do {
		
		string fileName = string(entry->d_name);

		if(!fileName.empty() && fileName != "." && fileName !=  ".." && fileName.find(".") != std::string::npos && fileName.find(".ini") == std::string::npos)
		{
			string name = string(entry->d_name);
			Logger::WriteLog(name + "\n");
			//cout << name << endl;
			string tempName = directory;
			tempName.append(entry->d_name);

			FileReader* file = new FileReader(tempName, isFile);
			if(isFile)
			{
				files.push_back(file);
				fileSizes.push_back(files.back()->fileStringSize);
			}
			else //This is probably a directory then
			{
				FindFiles(directory + fileName + "/");
			}
		}
		else if(fileName != "." && fileName !=  ".." && fileName.find(".ini") == std::string::npos)
		{
			FindFiles(directory + fileName + "/");
		}
					
	} while (entry = readdir(dir));
	closedir(dir);
#endif		
}

void Forest::CommandLineParser(int argc, char **argv)
{
	bool minEnter = false;
	bool maxEnter = false;
	bool fileEnter = false;
	bool threadsEnter = false;
	bool coverageTracking = false;
	//All files need to be placed in data folder relative to your executable
	string tempFileName = DATA_FOLDER;

	for (int i = 1; i < argc; i++)
	{
		string arg(argv[i]);
		locale loc;
		for (std::string::size_type j = 0; j < arg.length(); ++j)
		{
			arg[j] = std::tolower(arg[j], loc);
		}
		if (arg.compare("-min") == 0)
		{
			// We know the next argument *should* be the minimum pattern to display
			minimum = atoi(argv[i + 1]);
			minEnter = true;
			i++;
		}
		else if (arg.compare("-max") == 0)
		{
			// We know the next argument *should* be the maximum pattern to display
			maximum = atoi(argv[i + 1]);
			levelRecordings.resize(maximum);
			//mostCommonPatternCount.resize(maximum);
			//mostCommonPattern.resize(maximum);
			maxEnter = true;
			i++;
		}
		else if (arg.compare("-f") == 0)
		{
			bool isFile = false;
			// We know the next argument *should* be the filename
			string header = DATA_FOLDER;
			tempFileName.append(argv[i + 1]);
			string fileTest = argv[i + 1];

			if(fileTest.find('.') != string::npos && fileTest[0] != '-') 
			{
				files.push_back(new FileReader(tempFileName, isFile));
				fileSizes.push_back(files.back()->fileStringSize);
				i++;
			}
			else if(fileTest.find('.') == string::npos && fileTest[0] != '-')
			{
			#if defined(_WIN64) || defined(_WIN32)
				header = "../../../../Database/";
			#elif defined(__linux__)
				header = "../../Database/";
			#endif
				header.append(fileTest);
				header.append("/");

				//Access files with full path
				if(fileTest.find(":") != std::string::npos)
				{
					header = fileTest;
					header.append("/");
				}

				FindFiles(header);
				i++;
			}
			else
			{
				FindFiles(header);	
			}
			fileEnter = true;
		}
		else if (arg.compare("-v") == 0)
		{
			// We know the next argument *should* be the filename
			Logger::verbosity = atoi(argv[i + 1]);
			i++;
		}
		else if (arg.compare("-d") == 0)
		{
			displayEachLevelSearch = true;
		}
		else if (arg.compare("-c") == 0)
		{
			findBestThreadNumber = true;
		}
		else if (arg.compare("-threads") == 0)
		{
			// We know the next argument *should* be the maximum pattern to display
			numThreads = atoi(argv[i + 1]);
			threadsEnter = true;
			i++;
		}
		else if(arg.compare("-mem") == 0)
		{
			memoryBandwidthMB = atoi(argv[i + 1]);
			usingMemoryBandwidth = true;
			i++;
		}
		else if(arg.compare("-lev") == 0 )
		{
			levelToOutput = atoi(argv[i + 1]);
			i++;
		}
		else if(arg.compare("-his") == 0)
		{
			history = atoi(argv[i + 1]);
			i++;
		}
		else if(arg.compare("-ram") == 0)
		{
			usingPureRAM = true;
		}
		else if(arg.compare("-hd") == 0)
		{
			usingPureHD = true;
		}
		else if(arg.compare("-p") == 0)
		{
			patternToSearchFor = argv[i+1];
			maximum = patternToSearchFor.size();
			i++;
		}
		else if(arg.compare("-o") == 0)
		{
			minOccurrence = atoi(argv[i+1]);
			i++;
		}
		else if(arg.compare("-cov") == 0)
		{
			coverageTracking = true;
			i++;
		}
		else if(arg.compare("-s") == 0)
		{
			outlierScans = true;
		}
		else if(arg.compare("-help") == 0 || arg.compare("/?") == 0)
		{
			DisplayHelpMessage();
			do
			{
				cout << '\n' <<"Press the Enter key to continue." << endl;
			} while (cin.get() != '\n');
			exit(0);
		}
		else
		{
			cout << "incorrect command line format at : " << arg << endl;
			DisplayHelpMessage();
			do
			{
				cout << '\n' <<"Press the Enter key to continue." << endl;
			} while (cin.get() != '\n');
			exit(0);
		}
	}

	//Make maximum the largest if not entered
	if(!maxEnter)
	{
		maximum = -1;
	}

	if(outlierScans)
	{
		minOccurrence = -1;
	}

	//If no file is entered we exit because there is nothing to play with
	if (!fileEnter)
	{
		exit(0);
	}

	unsigned long concurentThreadsSupported = std::thread::hardware_concurrency();

	stringstream buff;
	buff << "Number of threads on machine: " << concurentThreadsSupported << endl;
	Logger::WriteLog(buff.str());
	cout << buff.str();

	////If max not specified then make the largest pattern the fileSize
	//if (!maxEnter)
	//{
	//	maximum = files[f]->fileStringSize;
	//}
	//If min not specified then make the smallest pattern of 0
	if (!minEnter)
	{
		minimum = 0;
	}
	//If numCores is not specified then we use number of threads supported cores plus the main thread
	if (!threadsEnter /*|| numThreads > concurentThreadsSupported*/)
	{
		numThreads = concurentThreadsSupported + 1;
	}

	int bestThreadCount = 0;
	double fastestTime = 1000000000.0f;
	testIterations = 0;
	if (findBestThreadNumber)
	{
		numThreads = 2;
		testIterations = concurentThreadsSupported;
	}
}

bool Forest::PredictHardDiskOrRAMProcessing(LevelPackage levelInfo, PListType sizeOfPrevPatternCount)
{
	//Break early if memory usage is predetermined by command line arguments
	if(usingPureRAM)
	{
		return false;
	}
	if(usingPureHD)
	{
		return true;
	}
	//main thread is a hardware thread so dispatch threads requested minus 1
	PListType threadsToDispatch = numThreads - 1;

	//predictedMemoryForLevelProcessing has to include memory loading in from previous level and memory for the next level
	//First calculate the size of the file because that is the maximum number of pLists we can store minus the level
	//Second calculate the size of Tree objects will be allocated based on the number of POTENTIAL PATTERNS...
	//POTENTIAL PATTERNS equals the previous list times 256 possible byte values but this value can't exceed the file size minus the current level
	PListType potentialPatterns = sizeOfPrevPatternCount*256;
	//cout << "Level " << levelInfo.currLevel << " had " << sizeOfPrevPatternCount << " patterns!" << endl;

	//cout << "Eradicated file indexes: " << eradicatedPatterns << endl;
	if(potentialPatterns > files[f]->fileStringSize - (levelInfo.currLevel - 1))
	{
		//Factor in eradicated patterns because those places no longer need to be checked in the file
		potentialPatterns = files[f]->fileStringSize - eradicatedPatterns;
		//cout << "Potential patterns has exceed file size so we downgrade to " << potentialPatterns << endl;
	}

	//cout << "Potential patterns for level " << levelInfo.currLevel << " is " << potentialPatterns << endl;


	PListType sizeOfTreeMap = 0;
	if(levelInfo.useRAM)
	{
		sizeOfTreeMap = sizeof(char)*potentialPatterns + sizeof(TreeRAM)*potentialPatterns;
	}
	else
	{
		sizeOfTreeMap = sizeof(char)*levelInfo.currLevel*potentialPatterns + sizeof(TreeHD)*potentialPatterns;
	}

	PListType vectorSize = sizeOfPrevPatternCount*sizeof(vector<PListType>*);
	PListType pListIndexesLeft = (files[f]->fileStringSize - eradicatedPatterns)*sizeof(PListType);
	PListType predictedMemoryForNextLevelMB = (PListType)((vectorSize + pListIndexesLeft + sizeOfTreeMap)/1000000.0f);

	//cout << "Predicted size for level " << levelInfo.currLevel << " is " << predictedMemoryForNextLevelMB << " MB" << endl;

	PListType previousLevelMemoryMB = 0;
	PListType predictedMemoryForLevelProcessing = 0;

	double prevMemoryMB = MemoryUtils::GetProgramMemoryConsumption() - MemoryUsageAtInception;
	if(prevMemoryMB > 0.0f)
	{
		previousLevelMemoryMB = (PListType)prevMemoryMB;
	}

	//cout << "Size used for previous level " << levelInfo.currLevel - 1 << " is " << previousLevelMemoryMB << " MB" << endl;

	predictedMemoryForLevelProcessing = previousLevelMemoryMB + predictedMemoryForNextLevelMB;

	if(predictedMemoryForLevelProcessing > memoryBandwidthMB)
	{

		stringstream stringbuilder;
		stringbuilder << "Using HARD DISK! Total size for level " << levelInfo.currLevel << " processing is " << predictedMemoryForLevelProcessing << " MB" << endl;
		cout << stringbuilder.str();
		Logger::WriteLog(stringbuilder.str());

		return true;
	}
	else
	{
		if(files[f]->fileString.size() == 0)
		{
			//new way to read in file
			files[f]->fileString.resize(files[f]->fileStringSize);
			files[f]->copyBuffer->read( &files[f]->fileString[0], files[f]->fileString.size());
		}

		//stringstream stringbuilder;
		//stringbuilder << "Using RAM! Total size for level " << levelInfo.currLevel << " processing is " << predictedMemoryForLevelProcessing << " MB" << endl;
		//cout << stringbuilder.str();
		//Logger::WriteLog(stringbuilder.str());

		return false;
	}
}

void Forest::PrepDataFirstLevel(bool prediction, vector<vector<string>>& fileList, vector<vector<PListType>*>* prevLocalPListArray, vector<vector<PListType>*>* globalLocalPListArray)
{
	PListType threadsToDispatch = numThreads - 1;
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
				fileIDMutex->lock();
				fileID++;
				threadFilesNames << "PListChunks" << fileID;
				fileIDMutex->unlock();

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
				if(!history)
				{
					DeleteArchives(tempFileList[i], ARCHIVE_FOLDER);
				}
			}
			//Transition to using entire file when first level was hard disk processing and next level is pure ram
			if(files[f]->fileString.size() != files[f]->fileStringSize)
			{
				//new way to read in file
				files[f]->copyBuffer->seekg( 0 );
				files[f]->fileString.resize(files[f]->fileStringSize);
				files[f]->copyBuffer->read( &files[f]->fileString[0], files[f]->fileString.size());
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
	PListType threadsToDispatch = numThreads - 1;

	if(prediction)
	{
		if(levelInfo.useRAM)
		{
			//chunk file
			PListArchive* threadFile;
			stringstream threadFilesNames;

			fileList.clear();

			threadFilesNames.str("");
			fileIDMutex->lock();
			fileID++;
			threadFilesNames << "PListChunks" << fileID;
			fileIDMutex->unlock();

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
			if(!history)
			{
				DeleteArchives(fileList, ARCHIVE_FOLDER);
			}
			fileList.clear();
		}
	}

	levelInfo.useRAM = !prediction;
	usedRAM[levelInfo.threadIndex] = !prediction;

	if(levelInfo.useRAM && files[f]->fileString.size() != files[f]->fileStringSize)
	{
		//new way to read in file
		files[f]->copyBuffer->seekg( 0 );
		files[f]->fileString.resize(files[f]->fileStringSize);
		files[f]->copyBuffer->read( &files[f]->fileString[0], files[f]->fileString.size());
	}
}

bool Forest::NextLevelTreeSearch(unsigned int level)
{

	unsigned int threadsToDispatch = numThreads - 1;

	LevelPackage levelInfo;
	levelInfo.currLevel = level;
	levelInfo.inceptionLevelLOL = 0;
	levelInfo.threadIndex = 0;
	levelInfo.useRAM = usedRAM[0];
	levelInfo.coreIndex = 0;

	//Do one prediction for them all
	bool prediction = PredictHardDiskOrRAMProcessing(levelInfo, levelRecordings[0]);

	vector<vector<string>> fileList = prevFileNameList;
	PrepDataFirstLevel(prediction, fileList, prevPListArray, globalPListArray);

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

#if defined(_WIN64) || defined(_WIN32)
			threadPool->push_back(std::async(std::launch::async, &Forest::ThreadedLevelTreeSearchRecursionList, this, prevPListArray, balancedTruncList[i], temp2, levelInfo));
#else
			threadPool->push_back(std::async(std::launch::async, &Forest::ThreadedLevelTreeSearchRecursionList, this, prevPListArray, balancedTruncList[i], temp2, levelInfo));
#endif
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
#if defined(_WIN64) || defined(_WIN32)
			threadPool->push_back(std::async(std::launch::async, &Forest::ThreadedLevelTreeSearchRecursionList, this, prevPListArray, temp, balancedTruncList[i], levelInfo));
#else
			threadPool->push_back(std::async(std::launch::async, &Forest::ThreadedLevelTreeSearchRecursionList, this, prevPListArray, temp, balancedTruncList[i], levelInfo));
#endif
		}
		countMutex->unlock();
		WaitForThreads(localWorkingThreads, threadPool);
	}
	
	prevPListArray->clear();
	prevPListArray->swap((*globalPListArray));

	DisplayPatternsFound();

	threadPool->erase(threadPool->begin(), threadPool->end());
	(*threadPool).clear();

	return false;
}

bool Forest::NextLevelTreeSearchRecursion(vector<vector<PListType>*>* prevLocalPListArray, vector<vector<PListType>*>* globalLocalPListArray, vector<string>& fileList, LevelPackage& levelInfo)
{
	bool localUsingRam = true;
	bool prediction = PredictHardDiskOrRAMProcessing(levelInfo, levelRecordings[levelInfo.currLevel - 2]);
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
#if defined(_WIN64) || defined(_WIN32)
				if (localThreadPool != NULL && (*localThreadPool)[localWorkingThreads[k]].valid())
#else	
				if (localThreadPool != NULL && (*localThreadPool)[localWorkingThreads[k]].valid())
				//if ((*localThreadPool)[localWorkingThreads[k]].wait_for(chrono::milliseconds(5)) == std::future_status::ready)
#endif	
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
			fileIDMutex->lock();
			fileID++;
			threadFilesNames << "PListChunks" << fileID;
			fileIDMutex->unlock();

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
			DeleteArchive(prevFileNames[prevChunkCount], ARCHIVE_FOLDER);

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
	unsigned int threadsToDispatch = numThreads - 1;

	vector<string> fileNamesBackup;
	for(int a = 0; a < fileNamesToReOpen.size(); a++)
	{
		fileNamesBackup.push_back(fileNamesToReOpen[a]);
	}

	PListType globalTotalMemoryInBytes = 0;
	PListType globalTotalLeafSizeInBytes = 0;

	vector<PListArchive*> patternFiles;
	PListType internalRemovedCount = 0;

	while(currentFile < fileNamesBackup.size())
	{
		memoryOverflow = false;

		vector<PListArchive*> archiveCollection;
		map<PatternType, vector<PListType>*> finalMetaDataMap;

		globalTotalLeafSizeInBytes = 0;
		globalTotalMemoryInBytes = 32;

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

			if(finalMetaDataMap.size() > 0)
			{

				globalTotalLeafSizeInBytes = (finalMetaDataMap.size() + 1)*32;
				globalTotalLeafSizeInBytes += (finalMetaDataMap.size() + 1)*(finalMetaDataMap.begin()->first.capacity() + sizeof(string));
				globalTotalLeafSizeInBytes += finalMetaDataMap.size() * 32;
			}

			if(((globalTotalLeafSizeInBytes/1000000.0f) + (globalTotalMemoryInBytes/1000000.0f) > 2.0f*memDivisor/1000000.0f || overMemoryCount) && finalMetaDataMap.size() > 0)
			{
				//stringstream crap;
				//crap << "Actual overflow " << MemoryUtils::GetProgramMemoryConsumption() - threadMemoryConsumptionInMB << " in MB!\n";
				//crap << "Quick approximation at Process Chunks And Generate of " << (globalTotalLeafSizeInBytes/1000000.0f) + (globalTotalMemoryInBytes/1000000.0f) << " in MB!\n";
				//Logger::WriteLog(crap.str());
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

					Logger::WriteLog("Purging the entire map mwuahahah!\n");

					//thread files
					PListArchive* currChunkFile = NULL;
					bool notBegun = true;
					PListType removedPatterns = 0;

					it_map_list_p_type iterator = finalMetaDataMap.begin();
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
								fileIDMutex->lock();
								fileID++;
								fileNameage << ARCHIVE_FOLDER << "PListChunks" << fileID << ".txt";
								fileNameForPrevList << "PListChunks" << fileID;
								fileIDMutex->unlock();

								prevFileNameList[threadNumber].push_back(fileNameForPrevList.str());

								currChunkFile = new PListArchive(fileNameForPrevList.str(), true);

								threadNumber++;
								threadNumber %= threadsToDispatch;
							}
							else
							{

								stringstream fileNameForPrevList;
								fileIDMutex->lock();
								fileID++;
								fileNameForPrevList << "PListChunks" << fileID;
								fileIDMutex->unlock();

								newFileNames.push_back(fileNameForPrevList.str());

								currChunkFile = new PListArchive(fileNameForPrevList.str(), true);
							}
						}

						if(iterator->second->size() >= minOccurrence /*|| (outlierScans && iterator->second->size() == 1)*/)
						{
							currChunkFile->WriteArchiveMapMMAP(*iterator->second);
							interimCount++;

							if(mostCommonPatternIndex.size() < currLevel)
							{
								mostCommonPatternIndex.resize(currLevel);
								mostCommonPatternCount.resize(currLevel);
							}

							if(iterator->second->size() > mostCommonPatternCount[currLevel - 1])
							{
								mostCommonPatternCount[currLevel - 1] = iterator->second->size();

								mostCommonPatternIndex[currLevel - 1] = (*iterator->second)[0] - currLevel;
							}

						}
						else
						{
							removedPatterns++;
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
							DeleteChunk(newFileNames[newFileNames.size() - 1], ARCHIVE_FOLDER);
							newFileNames.pop_back();
						}
					}
					finalMetaDataMap.clear();

					countMutex->lock();
					eradicatedPatterns += removedPatterns;
					countMutex->unlock();

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
								//Only add size because it is a new vector and add in capacity
								globalTotalMemoryInBytes += sizeof(PListType)*finalMetaDataMap[(*stringBuffer)[partialLists]]->capacity();
							}
						}
						else
						{
							//First subract original vector size in capacity
							globalTotalMemoryInBytes -= sizeof(PListType)*finalMetaDataMap[(*stringBuffer)[partialLists]]->capacity();
							finalMetaDataMap[(*stringBuffer)[partialLists]]->insert(finalMetaDataMap[(*stringBuffer)[partialLists]]->end(), packedPListArray[partialLists]->begin(), packedPListArray[partialLists]->end());
							delete packedPListArray[partialLists];
							packedPListArray[partialLists] = NULL;
							countAdded++;
							//then add new size in capacity
							globalTotalMemoryInBytes += sizeof(PListType)*finalMetaDataMap[(*stringBuffer)[partialLists]]->capacity();
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

				DeleteChunk(fileNameForLater, ARCHIVE_FOLDER);

				delete archiveCollection[a];
				delete stringBufferFile;

				//NEW TEST CODE 
				//packedPListSize = packedPListArray.size();
				//

				PListType newCount = packedPListSize - countAdded;
				if(newCount > 0/* && countAdded == 0*/)
				{
					fileIDMutex->lock();
					fileID++;
					stringstream namer;
					namer << "PListChunks" << fileID << "_" << newCount;
					fileIDMutex->unlock();

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
		PListType removedPatterns = 0;

		for(it_map_list_p_type iterator = finalMetaDataMap.begin(); iterator != finalMetaDataMap.end(); iterator++)
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
					fileIDMutex->lock();
					fileID++;
					fileNameage << ARCHIVE_FOLDER << "PListChunks" << fileID << ".txt";
					fileNameForPrevList << "PListChunks" << fileID;
					fileIDMutex->unlock();

					prevFileNameList[threadNumber].push_back(fileNameForPrevList.str());

					currChunkFile = new PListArchive(fileNameForPrevList.str(), true);

					threadNumber++;
					threadNumber %= threadsToDispatch;
				}
				else
				{

					stringstream fileNameForPrevList;
					fileIDMutex->lock();
					fileID++;
					fileNameForPrevList << "PListChunks" << fileID;
					fileIDMutex->unlock();

					newFileNames.push_back(fileNameForPrevList.str());

					currChunkFile = new PListArchive(fileNameForPrevList.str(), true);
				}
			}

			if(iterator->second->size() >= minOccurrence /*|| (outlierScans && iterator->second->size() == 1)*/)
			{
				currChunkFile->WriteArchiveMapMMAP(*iterator->second);
				interimCount++;

				if(mostCommonPatternIndex.size() < currLevel)
				{
					mostCommonPatternIndex.resize(currLevel);
					mostCommonPatternCount.resize(currLevel);
				}

				if(iterator->second->size() > mostCommonPatternCount[currLevel - 1])
				{
					mostCommonPatternCount[currLevel - 1] = iterator->second->size();

					mostCommonPatternIndex[currLevel - 1] = (*iterator->second)[0] - currLevel;
				}
			}
			else
			{
				removedPatterns++;
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
				//if(newFileNames.size() > 0)
				//{
					DeleteChunk(newFileNames[newFileNames.size() - 1], ARCHIVE_FOLDER);
					newFileNames.pop_back();
				//}
				//else
				//{
				//	cout << "Interesting..." << endl;
				//}
			}
		}

		countMutex->lock();
		eradicatedPatterns += removedPatterns;
		countMutex->unlock();

		for(int a = prevCurrentFile; a < currentFile; a++)
		{
			DeleteChunk(fileNamesBackup[a], ARCHIVE_FOLDER);
		}
		prevCurrentFile = currentFile;
	}

	countMutex->lock();
	if(levelRecordings.size() < currLevel)
	{
		levelRecordings.resize(currLevel);
	}
	levelRecordings[currLevel - 1] += interimCount;

	if(coverage.size() < currLevel)
	{
		coverage.resize(currLevel);
	}
	coverage[currLevel - 1] += ((float)(interimCount))/((float)files[f]->fileStringSize);

	if(currLevel > currentLevelVector[threadNum])
	{
		currentLevelVector[threadNum] = currLevel;
	}
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
	unsigned int threadsToDispatch = numThreads - 1;

	PListType currPatternCount = 0;
	//Approximate pattern count for this level
	if(currLevel == 1)
	{
		currPatternCount = 256;
	}
	else
	{
		currPatternCount = 256*levelRecordings[currLevel - 1];
	}


	map<string, PListArchive*> currChunkFiles;
	for(int a = 0; a < currPatternCount; a++)
	{
		stringstream fileNameage;
		stringstream fileNameForPrevList;
		fileIDMutex->lock();
		fileID++;
		fileNameage << ARCHIVE_FOLDER << "PListChunks" << fileID << ".txt";
		fileNameForPrevList << "PListChunks" << fileID;
		fileIDMutex->unlock();

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

	int removedPatterns = 0;
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
							patternCounts[pattern].first++;
						}
						else
						{
							patternCounts[pattern].first = 1;
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

			DeleteChunk(fileNameForLater, ARCHIVE_FOLDER);

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
			if(currChunkFiles[buff.str()]->mappingIndex > (2*sizeof(PListType)) /*currChunkFiles[buff.str()]->mappingIndex > 0*/)
			{
				empty = false;
				interimCount++;
				

				if(mostCommonPatternIndex.size() < currLevel)
				{
					mostCommonPatternIndex.resize(currLevel);
					mostCommonPatternCount.resize(currLevel);
				}
				
				if(patternCounts.find(buff.str()) != patternCounts.end() && patternCounts[buff.str()].first > mostCommonPatternCount[currLevel - 1])
				{
					mostCommonPatternCount[currLevel - 1] = patternCounts[buff.str()].first;

					mostCommonPatternIndex[currLevel - 1] = patternCounts[buff.str()].second - currLevel;
				}
			}
			else if(currChunkFiles[buff.str()]->mappingIndex == (2*sizeof(PListType)))
			{
				countMutex->lock();
				eradicatedPatterns++;
				countMutex->unlock();
			}

			string fileToDelete = currChunkFiles[buff.str()]->patternName;
			currChunkFiles[buff.str()]->CloseArchiveMMAP();
			delete currChunkFiles[buff.str()];

			if(fileNamesBackup.size() == currentFile && empty)
			{
				DeleteChunk(fileToDelete, ARCHIVE_FOLDER);
			}

			/*if(mostCommonPattern.size() < currLevel)
			{
				mostCommonPattern.resize(currLevel);
				mostCommonPatternCount.resize(currLevel);
			}*/


		}
	}
	//countMutex->lock();
	//eradicatedPatterns += 256 - interimCount;
	//countMutex->unlock();

	countMutex->lock();
	if(levelRecordings.size() < currLevel)
	{
		levelRecordings.resize(currLevel);
	}
	levelRecordings[currLevel - 1] += interimCount;

	if(coverage.size() < currLevel)
	{
		coverage.resize(currLevel);
	}
	coverage[currLevel - 1] += ((float)(interimCount))/((float)files[f]->fileStringSize);

	//stringstream buffy;
	//buffy << currLevel << " with a total of " << levelRecordings[currLevel - 1] << endl;
	//Logger::WriteLog(buffy.str());

	if(currLevel > currentLevelVector[threadNum])
	{
		currentLevelVector[threadNum] = currLevel;
	}

	countMutex->unlock();

	return interimCount;
}
bool Forest::ProcessHD(LevelPackage& levelInfo, vector<string>& fileList, bool &isThreadDefuncted)
{
	double threadMemoryConsumptionInMB = MemoryUtils::GetProgramMemoryConsumption();

	int threadNum = levelInfo.threadIndex;
	unsigned int currLevel = levelInfo.currLevel;
	PListType newPatternCount = 0;
	//Divide between file load and previous level pLists and leave some for new lists haha 
	PListType memDivisor = (PListType)(((memoryPerThread*1000000)/3.0f));

	bool morePatternsToFind = false;

	unsigned int fileIters = (unsigned int)(files[f]->fileStringSize/memDivisor);
	if(files[f]->fileStringSize%memDivisor != 0)
	{
		fileIters++;
	}

	vector<string> fileNamesToReOpen;
	string saveOffPreviousStringData = "";
	vector<string> newFileNames;

	PListType globalTotalMemoryInBytes = 0;
	PListType globalTotalLeafSizeInBytes = 0;
	try
	{
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
							if(files[f]->fileStringSize <= memDivisor*j + memDivisor)
							{
								patternCount = files[f]->fileStringSize - memDivisor*j;
							}
							else
							{
								patternCount = memDivisor;
							}


							countMutex->lock();
							bool foundChunkInUse = false;
							for(it_chunk iterator = chunkIndexToFileChunk.begin(); iterator != chunkIndexToFileChunk.end(); iterator++)
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
								FileReader fileReaderTemp(files[f]->fileName, isFile, true);
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

						TreeHD leaf;
						//Start over
						globalTotalLeafSizeInBytes = 0;
						globalTotalMemoryInBytes = 32;

						bool justPassedMemorySize = false;

						for(PListType i = 0; i < packedListSize; i++)
						{
							vector<PListType>* pList = packedPListArray[i];
							PListType pListLength = packedPListArray[i]->size();
							PListType k = prevLeafIndex[i];

							if(leaf.leaves.size() > 0)
							{
								//Size needed for each node in the map overhead essentially
								globalTotalLeafSizeInBytes = (leaf.leaves.size() + 1)*32;
								globalTotalLeafSizeInBytes += (leaf.leaves.size() + 1)*(leaf.leaves.begin()->first.capacity() + sizeof(string));
								globalTotalLeafSizeInBytes += leaf.leaves.size() * 32;
								//Size of TreeHD pointer
								globalTotalLeafSizeInBytes += leaf.leaves.size() * 8;

							}

							if(((globalTotalLeafSizeInBytes/1000000.0f) + (globalTotalMemoryInBytes/1000000.0f)) < (memDivisor/1000000.0f)/* && !overMemoryCount*/)
							{
								/*stringstream crap;
								crap << "Approximation overflow at Process HD of " << sizeInMB << " in MB!\n";
								crap << "Overflow at Process HD of " << currMemoryOverflow << " in MB!\n";
								Logger::WriteLog(crap.str());*/
								signed long long relativeIndex = 0;
								PListType indexForString = 0;
								while( k < pListLength && ((*pList)[k]) < (j+1)*memDivisor )
								{
									if(!writingFlag)
									{

										try
										{
											if(((*pList)[k]) < files[f]->fileStringSize)
											{
												//If index comes out to be larger than fileString than it is a negative number 
												//and we must use previous string data!
												if(((((*pList)[k]) - memDivisor*j) - (currLevel-1)) >= files[f]->fileStringSize)
												{
													relativeIndex = ((((*pList)[k]) - memDivisor*j) - (currLevel-1));
													string pattern = "";
													relativeIndex *= -1;
													indexForString = saveOffPreviousStringData.size() - relativeIndex;
													if(saveOffPreviousStringData.size() > 0 && relativeIndex > 0)
													{

														pattern = saveOffPreviousStringData.substr(indexForString, relativeIndex);
														pattern.append(fileChunks[threadChunkToUse].substr(0, currLevel - pattern.size()));

														if(patternToSearchFor.size() == 0 || pattern[pattern.size() - 1] == patternToSearchFor[levelInfo.currLevel - 1])
														{
															if(leaf.leaves.find(pattern) != leaf.leaves.end())
															{
																globalTotalMemoryInBytes -= leaf.leaves[pattern].pList.capacity()*sizeof(PListType);
															}
															leaf.addLeaf((*pList)[k]+1, pattern);

															globalTotalMemoryInBytes += leaf.leaves[pattern].pList.capacity()*sizeof(PListType);
														}

													}
												}
												else
												{
													//If pattern is past end of string stream then stop counting this pattern
													if(((*pList)[k]) < files[f]->fileStringSize)
													{

														string pattern = fileChunks[threadChunkToUse].substr(((((*pList)[k]) - memDivisor*j) - (currLevel-1)), currLevel);

														if(patternToSearchFor.size() == 0 || pattern[pattern.size() - 1] == patternToSearchFor[levelInfo.currLevel - 1])
														{
															leaf.addLeaf((*pList)[k]+1, pattern);
															globalTotalMemoryInBytes += sizeof(PListType);
														}
													}
													else if(((((*pList)[k]) - memDivisor*j) - (currLevel-1)) < 0)
													{
														cout << "String access is out of bounds at beginning" << endl;
													}
													else if((((*pList)[k]) - memDivisor*j) >= files[f]->fileStringSize)
													{
														cout << "String access is out of bounds at end" << endl;
													}
												}
											}
											else
											{
												eradicatedPatterns++;
												//cout << "don't pattern bro at this index: " << ((*pList)[k]) << endl;
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

								globalTotalLeafSizeInBytes = 0;
								globalTotalMemoryInBytes = 32;
								//if true already do not write again until memory is back in our hands
								if(!justPassedMemorySize && leaf.leaves.size() > 0)
								{

									PListType patterns = leaf.leaves.size();

									justPassedMemorySize = true;
									stringstream stringBuilder;
									fileIDMutex->lock();
									fileID++;
									stringBuilder << fileID;
									fileIDMutex->unlock();
									fileNamesToReOpen.push_back(CreateChunkFile(stringBuilder.str(), leaf, levelInfo));

								}
								else
								{

									//If memory is unavailable sleep for one second
									//std::this_thread::sleep_for (std::chrono::seconds(1));
								}
								i--;
							}
						}

						if(packedListSize > 0 && leaf.leaves.size() > 0)
						{

							globalTotalLeafSizeInBytes = 0;
							globalTotalMemoryInBytes = 32;

							stringstream stringBuilder;
							fileIDMutex->lock();
							fileID++;
							stringBuilder << fileID;
							fileIDMutex->unlock();
							fileNamesToReOpen.push_back(CreateChunkFile(stringBuilder.str(), leaf, levelInfo));

						}
					}
					
					for(PListType pTits = 0; pTits < packedPListArray.size(); pTits++)
					{
						delete packedPListArray[pTits];
					}
				}
			}
			archive.CloseArchiveMMAP();
		}

		if(levelToOutput == 0 || (levelToOutput != 0 && currLevel >= levelToOutput))
		{
			newPatternCount += ProcessChunksAndGenerate(fileNamesToReOpen, newFileNames, memDivisor, threadNum, currLevel, levelInfo.coreIndex);
		}
		//Logger::WriteLog("Eradicated patterns: " + std::to_string(eradicatedPatterns) + "\n");
	}
	catch(exception e)
	{
		cout << e.what() << endl;
		MemoryUtils::print_trace();
	}

	if(!history)
	{
		DeleteArchives(fileList, ARCHIVE_FOLDER);
	}

	fileList.clear();
	for(int i = 0; i < newFileNames.size(); i++)
	{
		fileList.push_back(newFileNames[i]);
	}

	newFileNames.clear();

	if(fileList.size() > 0 && levelInfo.currLevel < maximum)
	{
		morePatternsToFind = true;
		levelInfo.currLevel++;

		DispatchNewThreadsHD(newPatternCount, morePatternsToFind, fileList, levelInfo, isThreadDefuncted);

	}
	else
	{
		DeleteArchives(fileList, ARCHIVE_FOLDER);
	}
	return morePatternsToFind;
}

bool Forest::DispatchNewThreadsRAM(PListType newPatternCount, bool& morePatternsToFind, vector<PListType> &linearList, vector<PListType> &pListLengths, LevelPackage levelInfo, bool& isThreadDefuncted)
{
	bool dispatchedNewThreads = false;
	bool alreadyUnlocked = false;
	countMutex->lock();

	int threadsToDispatch = numThreads - 1;
	int unusedCores = (threadsToDispatch - (threadsDispatched - threadsDefuncted)) + 1;
	
	if(pListLengths.size() < unusedCores && unusedCores > 1)
	{
		unusedCores = (int)pListLengths.size();
	}
	//Be conservative with thread allocation
	//Only create new thread for work if the new job will have atleast 10 patterns
	//Stack overflow can occur if there are too many little jobs being assigned
	//Need to have an available core, need to still have patterns to search and need to have more than 1 pattern to be worth splitting up the work
	if(unusedCores > 1 && morePatternsToFind && pListLengths.size()/unusedCores > 10)
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
#if defined(_WIN64) || defined(_WIN32)
						localThreadPool->push_back(std::async(std::launch::async, &Forest::ThreadedLevelTreeSearchRecursionList, this, prevLocalPListArray, balancedTruncList[i], vector<string>(), levelInfoRecursion));
#else
						localThreadPool->push_back(std::async(std::launch::async, &Forest::ThreadedLevelTreeSearchRecursionList, this, prevLocalPListArray, balancedTruncList[i], vector<string>(), levelInfoRecursion));
#endif
					}
					countMutex->unlock();

					alreadyUnlocked = true;
					WaitForThreads(localWorkingThreads, localThreadPool, true, levelInfo.threadIndex);

					localThreadPool->erase(localThreadPool->begin(), localThreadPool->end());
					(*localThreadPool).clear();
					delete localThreadPool;
					morePatternsToFind = false;
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

	int threadsToDispatch = numThreads - 1;
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

bool Forest::ProcessRAM(vector<vector<PListType>*>* prevLocalPListArray, vector<vector<PListType>*>* globalLocalPListArray, LevelPackage& levelInfo, bool &isThreadDefuncted)
{
	//Keeps track of all pLists in one contiguous block of memory
	vector<PListType> linearList;
	//Keeps track of the length of each pList
	vector<PListType> pListLengths;

	//Keeps track of all pLists in one contiguous block of memory
	vector<PListType> prevLinearList;
	//Keeps track of the length of each pList
	vector<PListType> prevPListLengths;

	bool continueSearching = true;
	int threadsToDispatch = numThreads - 1;
	PListType totalTallyRemovedPatterns = 0;
	PListType newPatterns = 0;
	string globalStringConstruct;
	vector<PListType> pListTracker;
	PListType fileSize = files[f]->fileStringSize;
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
	linearList.reserve(totalCount);

	PListType linearListIndex = 0;

	PListType globalStride = 0;

	//We have nothing to process!
	if(totalCount == 0)
		return false;

	while(continueSearching)
	{

		PListType stride = 0;
		PListType strideCount = 0;
		PListType prevStride = 0;

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
						globalStringConstruct[stringIndexer++] = files[f]->fileString[prevLinearList[i]];

						stride += abs((long long)(prevLinearList[i] - prevStride));
						strideCount++;
						prevStride = prevLinearList[i];
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
						globalStringConstruct[stringIndexer++] = files[f]->fileString[prevLinearList[i]];

						stride += abs((long long)(prevLinearList[i] - prevStride));
						strideCount++;
						prevStride = prevLinearList[i];
					}
				}
			}
		}
		
		if(globalStride == 0)
		{
			globalStride = (stride / strideCount);
		}
		else
		{
			globalStride += (stride / strideCount);
			globalStride /= 2.0f;
		}

		if(prevPListLengths.size() == 0)
		{
			continueSearching = false;
			break;
		}

		globalStringConstruct.resize(stringIndexer);
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
						if (insert >= minOccurrence)
						{
							pListLengths.push_back(newPList[indexesToPush[k]].size());
							linearList.insert(linearList.end(), newPList[indexesToPush[k]].begin(), newPList[indexesToPush[k]].end());

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
						if (insert >= minOccurrence)
						{
							pListLengths.push_back(newPList[indexesToPush[k]].size());
							linearList.insert(linearList.end(), newPList[indexesToPush[k]].begin(), newPList[indexesToPush[k]].end());

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

		eradicatedPatterns += totalTallyRemovedPatterns;

		if(levelRecordings.size() < levelInfo.currLevel)
		{
			levelRecordings.resize(levelInfo.currLevel);
		}
		levelRecordings[levelInfo.currLevel - 1] += pListLengths.size();

		if(mostCommonPatternIndex.size() < levelInfo.currLevel)
		{
			mostCommonPatternIndex.resize(levelInfo.currLevel);
			mostCommonPatternCount.resize(levelInfo.currLevel);
		}
		
		PListType countage = 0;
		PListType indexOfList = 0;
		bool chosen = false;
		for (PListType i = 0; i < pListLengths.size(); i++)
		{
			if(pListLengths[i] > 0)
			{
				if( pListLengths[i] > mostCommonPatternCount[levelInfo.currLevel - 1])
				{
					mostCommonPatternCount[levelInfo.currLevel - 1] = pListLengths[i];
					mostCommonPatternIndex[levelInfo.currLevel - 1] = linearList[countage] - levelInfo.currLevel;
					indexOfList = countage;
					chosen = true;
				}
			}
			countage += pListLengths[i];
		}

		if(coverage.size() < levelInfo.currLevel)
		{
			coverage.resize(levelInfo.currLevel);
		}

		if(chosen)
		{
			//Monitor number of patterns that do not overlap ie coverage
			PListType index = indexOfList;
			PListType count = mostCommonPatternCount[levelInfo.currLevel - 1];
			PListType totalTally = 0;
			PListType prevIndex = 0;
			if(count > 1)
			{
				prevIndex = linearList[index];
				totalTally++;
		
				for(PListType i = index + 1; i < count + index; i++)
				{
					PListType span = linearList[i] - prevIndex;
					if(span >= levelInfo.currLevel)
					{
						PListType pIndex = linearList[i] - levelInfo.currLevel;
						totalTally++;
						prevIndex = pIndex;
					}
				}
			}
			//Coverage of most common pattern per level
			if(totalTally * levelInfo.currLevel > coverage[levelInfo.currLevel - 1])
			{
				coverage[levelInfo.currLevel - 1] = ((double)(totalTally * levelInfo.currLevel)) / ((double)(files[f]->fileStringSize));
			}
			if(totalTally != count)
			{
				//cout << "Number of overlapping patterns: " << count - totalTally << endl;
			}
		}

		levelInfo.currLevel++;

		if(levelInfo.currLevel > currentLevelVector[levelInfo.threadIndex])
		{
			currentLevelVector[levelInfo.threadIndex] = levelInfo.currLevel;
		}


		countMutex->unlock();

		if(linearList.size() == 0 || levelInfo.currLevel - 1 >= maximum)
		{
			prevLinearList.clear();
			prevLinearList.reserve(linearList.size());
			prevLinearList.swap((linearList));

			prevPListLengths.clear();
			prevPListLengths.reserve(pListLengths.size());
			prevPListLengths.swap((pListLengths));
			pListLengths.reserve(0);

			continueSearching = false;
		}
		else
		{
			//Have to add prediction here 
			bool prediction = PredictHardDiskOrRAMProcessing(levelInfo, levelRecordings[levelInfo.currLevel - 2]);
			if(prediction)
			{

				PListType indexing = 0;
				prevLocalPListArray->clear();
				for(int piss = 0; piss < pListLengths.size(); piss++)
				{
					prevLocalPListArray->push_back(new vector<PListType>(linearList.begin() + indexing, linearList.begin() + indexing + pListLengths[piss]));
					indexing += pListLengths[piss];
				}

				linearList.clear();
				linearList.reserve(0);
				pListLengths.clear();
				pListLengths.reserve(0);
				break;
			}
			else
			{
				continueSearching = true;
				
				DispatchNewThreadsRAM(0, continueSearching, linearList, pListLengths, levelInfo, isThreadDefuncted);

				prevLinearList.clear();
				prevLinearList.reserve(linearList.size());
				prevLinearList.swap((linearList));

				prevPListLengths.clear();
				prevPListLengths.reserve(pListLengths.size());
				prevPListLengths.swap((pListLengths));
				pListLengths.reserve(0);
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
	int threadsToDispatch = numThreads - 1;

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

	bool continueSearching = true;

	while(continueSearching)
	{
		bool useRAMBRO = true;
		if(levelInfo.currLevel != 2)
		{
			useRAMBRO = !NextLevelTreeSearchRecursion(prevLocalPListArray, globalLocalPListArray, fileList, levelInfo);
		}
		else
		{
			useRAMBRO = levelInfo.useRAM;
		}

		if(useRAMBRO)
		{
			continueSearching = ProcessRAM(prevLocalPListArray, globalLocalPListArray, levelInfo, isThreadDefuncted);
		}
		else
		{
			continueSearching = ProcessHD(levelInfo, fileList, isThreadDefuncted);
		}
	}

	if(prevLocalPListArray != NULL)
	{
		/*for(PListType i = 0; i < prevLocalPListArray->size(); i++)
		{
			if((*prevLocalPListArray)[i] != NULL)
			{
				delete (*prevLocalPListArray)[i];
			}
		}*/
		delete prevLocalPListArray;
	}

	if(globalLocalPListArray != NULL)
	{
		delete globalLocalPListArray;
	}

	countMutex->lock();
	if(currentLevelVector[levelInfo.threadIndex] > globalLevel)
	{
		globalLevel = currentLevelVector[levelInfo.threadIndex];
	}
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
	levelInfo.currLevel = globalLevel;
	levelInfo.coreIndex = threadNum;
	levelInfo.threadIndex = threadNum;

	int threadsToDispatch = numThreads - 1;
	PListType earlyApproximation = files[f]->fileString.size()/(256*(numThreads - 1));
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
#ifdef INTEGERS

		stringstream finalValue;
		//If pattern is past end of string stream then stop counting this pattern
		if (i < files[f]->fileStringSize)
		{
			while(i < files[f]->fileStringSize)
			{
				unsigned char value = files[f]->fileString[i];
				//if values are between 0 through 9 and include 45 for the negative sign
				if(value >= 48 && value <= 57 || value == 45)
				{
					finalValue << value;
				}


				if(value == '\r' || value == 13 || value == '\n' || value == ' ' || value == '/t')
				{
					while((value < 48 || value > 57) && value != 45 && i < files[f]->fileStringSize)
					{
						value = files[f]->fileString[i];
						i++;
					}
					if(i < files[f]->fileStringSize)
					{
						i-=2;
					}
					break;
				}
				else
				{
					i++;
				}
			}
			if(finalValue.str() != "")
			{
				string patternValue = finalValue.str();
				unsigned long long ull = stoull(patternValue, &sz);
				//cout << "Pattern found: " << ull << endl;
				leaf->addLeaf(ull, i + 1, patternValue);
			}
		}
#endif
#ifdef BYTES

		int temp = i + positionInFile + 1;
		uint8_t tempIndex = (uint8_t)files[f]->fileString[i];
		if(patternToSearchFor.size() == 0 || files[f]->fileString[i] == patternToSearchFor[0])
		{
			leaves[tempIndex]->push_back(temp);
			counting++;
		}
		if(overMemoryCount && counting >= PListArchive::hdSectorSize)
		{
			stringstream stringBuilder;
			fileIDMutex->lock();
			fileID++;
			stringBuilder << fileID;
			fileIDMutex->unlock();
			newFileNameList[threadNum].push_back(CreateChunkFile(stringBuilder.str(), leaves, levelInfo));
			for(int i = 0; i < 256; i++)
			{
				leaves[i] = new vector<PListType>();
				leaves[i]->reserve(earlyApproximation);
			}
		}
#endif
	}

	stringstream stringBuilder;
	fileIDMutex->lock();
	fileID++;
	stringBuilder << fileID;
	fileIDMutex->unlock();
	newFileNameList[threadNum].push_back(CreateChunkFile(stringBuilder.str(), leaves, levelInfo));
	
	return;
}

void Forest::PlantTreeSeedThreadRAM(PListType positionInFile, PListType startPatternIndex, PListType numPatternsToSearch, PListType threadIndex)
{
	int threadsToDispatch = numThreads - 1;
	PListType earlyApproximation = files[f]->fileString.size()/(256*(numThreads - 1));
	vector<PListType>* leaves[256];
	for(int i = 0; i < 256; i++)
	{
		leaves[i] = new vector<PListType>();
		leaves[i]->reserve(earlyApproximation);
	}
	PListType endPatternIndex = numPatternsToSearch + startPatternIndex;

	for (PListType i = startPatternIndex; i < endPatternIndex; i++)
	{
#ifdef INTEGERS

		stringstream finalValue;
		//If pattern is past end of string stream then stop counting this pattern
		if (i < files[f]->fileStringSize)
		{
			while(i < files[f]->fileStringSize)
			{
				unsigned char value = files[f]->fileString[i];
				//if values are between 0 through 9 and include 45 for the negative sign
				if(value >= 48 && value <= 57 || value == 45)
				{
					finalValue << value;
				}


				if(value == '\r' || value == 13 || value == '\n' || value == ' ' || value == '/t')
				{
					while((value < 48 || value > 57) && value != 45 && i < files[f]->fileStringSize)
					{
						value = files[f]->fileString[i];
						i++;
					}
					if(i < files[f]->fileStringSize)
					{
						i-=2;
					}
					break;
				}
				else
				{
					i++;
				}
			}
			if(finalValue.str() != "")
			{
				string patternValue = finalValue.str();
				unsigned long long ull = stoull(patternValue, &sz);
				//cout << "Pattern found: " << ull << endl;
				leaf->addLeaf(ull, i + 1, patternValue);
			}
		}
#endif
#ifdef BYTES

		int temp = i + positionInFile + 1;
		uint8_t tempIndex = (uint8_t)files[f]->fileString[i];
		if(patternToSearchFor.size() == 0 || files[f]->fileString[i] == patternToSearchFor[0])
		{
			leaves[tempIndex]->push_back(temp);
		}
#endif
	}

	for(int i = 0; i < 256; i++)
	{
		(*prevPListArray)[threadIndex + i*threadsToDispatch] = leaves[i];
	}
	
	currentLevelVector[threadIndex] = 2;
}

string Forest::CreateChunkFile(string fileName, TreeHD& leaf, LevelPackage levelInfo)
{
	string fileNameToReOpen;

	stringstream archiveName;
	string archiveFileType = "PListChunks";

	archiveName << archiveFileType << fileName << "_" << leaf.leaves.size();

	PListArchive* archiveCollective = new PListArchive(archiveName.str(), true);
	fileNameToReOpen = archiveName.str();
	typedef std::map<string, TreeHD>::iterator it_type;

	it_type iterator = leaf.leaves.begin();
	while(iterator != leaf.leaves.end()) 
	{
		archiveCollective->WriteArchiveMapMMAP(iterator->second.pList, iterator->first, false);
		// a full 2MB has to be written to disk before it is worth flushing otherwise there is a major slow down effect from constantly spawning hd flush sync threads
		//if(archiveCollective->totalWritten >= PListArchive::writeSize && overMemoryCount) 
		//{
		//	archiveCollective->WriteArchiveMapMMAP(vector<PListType>(), "", true);
		//}
		iterator = leaf.leaves.erase(iterator);
	}
	map<string, TreeHD> test;
	test.swap(leaf.leaves);
	leaf.pList.clear();

	archiveCollective->DumpPatternsToDisk(levelInfo.currLevel);
	archiveCollective->WriteArchiveMapMMAP(vector<PListType>(), "", true);
	archiveCollective->CloseArchiveMMAP();

	delete archiveCollective;

	return fileNameToReOpen;
}

PListArchive* Forest::CreateChunkFileHandle(string fileName, TreeHD& leaf, LevelPackage levelInfo)
{
	string fileNameToReOpen;

	stringstream archiveName;
	string archiveFileType = "PListChunks";

	archiveName << archiveFileType << fileName << "_" << leaf.leaves.size();

	PListArchive* archiveCollective = new PListArchive(archiveName.str(), true);
	fileNameToReOpen = archiveName.str();
	typedef std::map<string, TreeHD>::iterator it_type;

	it_type iterator = leaf.leaves.begin();
	while(iterator != leaf.leaves.end()) 
	{
		archiveCollective->WriteArchiveMapMMAP(iterator->second.pList, iterator->first, false);
		// a full 2MB has to be written to disk before it is worth flushing otherwise there is a major slow down effect from constantly spawning hd flush sync threads
		if(archiveCollective->totalWritten >= PListArchive::writeSize && overMemoryCount) 
		{
			archiveCollective->WriteArchiveMapMMAP(vector<PListType>(), "", true);
		}
		iterator = leaf.leaves.erase(iterator);
	}
	map<string, TreeHD> test;
	test.swap(leaf.leaves);
	leaf.pList.clear();

	archiveCollective->DumpPatternsToDisk(levelInfo.currLevel);
	archiveCollective->WriteArchiveMapMMAP(vector<PListType>(), "", true/*, true*/);
	//archiveCollective->CloseArchiveMMAP();

	//delete archiveCollective;

	return archiveCollective;
}

string Forest::CreateChunkFile(string fileName, vector<vector<PListType>*> leaves, LevelPackage levelInfo)
{
	string fileNameToReOpen;

	stringstream archiveName;
	string archiveFileType = "PListChunks";

	archiveName << archiveFileType << fileName << "_256";

	PListArchive* archiveCollective = new PListArchive(archiveName.str(), true);
	fileNameToReOpen = archiveName.str();

	stringstream stringbuilder;

	for(int i = 0; i < 256; i++)
	{
		if(leaves[i]->size() > 0)
		{
			stringbuilder << ((char)i);
			archiveCollective->WriteArchiveMapMMAP(*leaves[i], stringbuilder.str(), false);
			// a full 2MB has to be written to disk before it is worth flushing otherwise there is a major slow down effect from constantly spawning hd flush sync threads
			if(archiveCollective->totalWritten >= PListArchive::writeSize && overMemoryCount) 
			{
				archiveCollective->WriteArchiveMapMMAP(vector<PListType>(), "", true);
			}

			stringbuilder.str("");
			delete leaves[i];
		}
	}
	
	archiveCollective->DumpPatternsToDisk(levelInfo.currLevel);
	archiveCollective->WriteArchiveMapMMAP(vector<PListType>(), "", true);
	archiveCollective->CloseArchiveMMAP();

	delete archiveCollective;

	return fileNameToReOpen;
}

void Forest::DeleteChunks(vector<string> fileNames, string folderLocation)
{
	for(int i = 0; i < fileNames.size(); i++)
	{
		string fileNameToBeRemoved = folderLocation;
		fileNameToBeRemoved.append(fileNames[i].c_str());
		fileNameToBeRemoved.append(".txt");

		//filesToBeRemovedLock.lock();
		//filesToBeRemoved.push(fileNameToBeRemoved);
		//filesToBeRemovedLock.unlock();

		if(remove( fileNameToBeRemoved.c_str()) != 0)
		{
			stringstream builder;
			builder << "Archives Failed to delete '" << fileNameToBeRemoved << "': " << strerror(errno) << '\n';
			Logger::WriteLog(builder.str());
		}
		else
		{
			//stringstream builder;
			//builder << "Archives succesfully deleted " << fileNameToBeRemoved << '\n';
			//Logger::WriteLog(builder.str());
		}

		string fileNameToBeRemovedPatterns = folderLocation;
		fileNameToBeRemovedPatterns.append(fileNames[i].c_str());
		fileNameToBeRemovedPatterns.append("Patterns.txt");

		//filesToBeRemovedLock.lock();
		//filesToBeRemoved.push(fileNameToBeRemovedPatterns);
		//filesToBeRemovedLock.unlock();

		if(remove( fileNameToBeRemovedPatterns.c_str() ) != 0)
		{
			stringstream builder;
			builder << "Archives Failed to delete '" << fileNameToBeRemovedPatterns << "': " << strerror(errno) << '\n';
			Logger::WriteLog(builder.str());
		}
		else
		{
			//stringstream builder;
			//builder << "Archives succesfully deleted " << fileNameToBeRemovedPatterns << '\n';
			//Logger::WriteLog(builder.str());
		}
	}
}

void Forest::DeleteArchives(vector<string> fileNames, string folderLocation)
{
	for(int i = 0; i < fileNames.size(); i++)
	{
		string fileNameToBeRemoved = folderLocation;
		fileNameToBeRemoved.append(fileNames[i].c_str());
		fileNameToBeRemoved.append(".txt");

		//filesToBeRemovedLock.lock();
		//filesToBeRemoved.push(fileNameToBeRemoved);
		//filesToBeRemovedLock.unlock();

		if( remove( fileNameToBeRemoved.c_str() ) != 0)
		{
			stringstream builder;
			builder << "Archives Failed to delete '" << fileNameToBeRemoved << "': " << strerror(errno) << '\n';
			Logger::WriteLog(builder.str());
		}
		else
		{
			//stringstream builder;
			//builder << "Archives succesfully deleted " << fileNameToBeRemoved << '\n';
			//Logger::WriteLog(builder.str());
		}
	}
}

void Forest::DeleteArchive(string fileNames, string folderLocation)
{
	string fileNameToBeRemoved = folderLocation;
	fileNameToBeRemoved.append(fileNames.c_str());
	fileNameToBeRemoved.append(".txt");

	//filesToBeRemovedLock.lock();
	//filesToBeRemoved.push(fileNameToBeRemoved);
	//filesToBeRemovedLock.unlock();

	if( remove( fileNameToBeRemoved.c_str() ) != 0)
	{
		stringstream builder;
		builder << "Archive Failed to delete '" << fileNameToBeRemoved << "': " << strerror(errno) << '\n';
		Logger::WriteLog(builder.str());
	}
	else
	{
		//stringstream builder;
		//builder << "Archive succesfully deleted " << fileNameToBeRemoved << '\n';
		//Logger::WriteLog(builder.str());
	}
}
void Forest::DeleteChunk(string fileChunkName, string folderLocation)
{

	string fileNameToBeRemoved = folderLocation;
	fileNameToBeRemoved.append(fileChunkName.c_str());
	fileNameToBeRemoved.append(".txt");
	
	//filesToBeRemovedLock.lock();
	//filesToBeRemoved.push(fileNameToBeRemoved);
	//filesToBeRemovedLock.unlock();

	if( remove( fileNameToBeRemoved.c_str() ) != 0)
	{
		stringstream builder;
		builder << "Chunk Failed to delete " << fileNameToBeRemoved << ": " << strerror(errno) << '\n';
		Logger::WriteLog(builder.str());
	}
	else
	{
		//stringstream builder;
		//builder << "Chunk succesfully deleted " << fileNameToBeRemoved << '\n';
		//Logger::WriteLog(builder.str());
	}

	string fileNameToBeRemovedPatterns = folderLocation;
	fileNameToBeRemovedPatterns.append(fileChunkName.c_str());
	fileNameToBeRemovedPatterns.append("Patterns.txt");
	
	//filesToBeRemovedLock.lock();
	//filesToBeRemoved.push(fileNameToBeRemovedPatterns);
	//filesToBeRemovedLock.unlock();

	if( remove( fileNameToBeRemovedPatterns.c_str() ) != 0)
	{
		stringstream builder;
		builder << "Chunk Failed to delete '" << fileNameToBeRemovedPatterns << "': " << strerror(errno) << '\n';
		Logger::WriteLog(builder.str());
	}
	else
	{
		//stringstream builder;
		//builder << "Chunk succesfully deleted " << fileNameToBeRemovedPatterns << '\n';
		//Logger::WriteLog(builder.str());
	}

}
