#include "Forest.h"
#include "MemoryUtils.h"
#include <locale>
#include <list>
#include <algorithm>
#if defined(_WIN64) || defined(_WIN32)
	#include <direct.h>
#elif defined(__linux__)
	#include "sys/stat.h"
#endif

bool Forest::outlierScans = false;
bool Forest::overMemoryCount = false;
Forest::Forest(int argc, char **argv)
{
#if defined(_WIN64) || defined(_WIN32)
	system("del ..\\Log\\PList*.txt");
#elif defined(__linux__)
	system("rm -r ../Log/PList*");
#endif

	writingFlag = false;
	globalUsingRAM = false;
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
	sizeOfPreviousLevelMB = 0;
	eradicatedPatterns = 0;
	usingPureRAM = false;
	usingPureHD = false;
	startingLevel = 1;
	//Default pattern occurence size to 2
	minOccurrence = 2;

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
	localPrevPListArrayTriplet = new vector<vector<vector<PListType>*>*>();
	localGlobalPListArrayTriplet = new vector<vector<vector<PListType>*>*>();

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

	usingFileChunk.resize(threadsToDispatch, -1);
	chunkOrigin.resize(threadsToDispatch, -1);
	
	//Assume start with RAM
	usedRAM.resize(threadsToDispatch);
	for(int i = 0; i < threadsToDispatch; i++)
	{
		usedRAM[i] = true;
	}

	memoryCeiling = (PListType)MemoryUtils::GetAvailableRAMMB() - 1000;

	//If memory bandwidth not an input
	if(!usingMemoryBandwidth)
	{
		
		//Leave 1 GB to spare for operating system in case our calculations suck
		memoryBandwidthMB = memoryCeiling - 1000;
	}

	thread *memoryQueryThread = NULL;
	//if(!usingPureRAM)
	//{
		//Kick off thread that processes how much memory the program uses at a certain interval
		memoryQueryThread = new thread(&Forest::MemoryQuery, this);
	//}
	thread *msyncThread = NULL;
	if(!usingPureRAM)
	{
		//Kick off thread that processes how much memory the program uses at a certain interval
		msyncThread = new thread(&Forest::MonitorMSYNCThreads, this);
	}

	double threadMemoryConsumptionInMB = MemoryUtils::GetProgramMemoryConsumption();
	stringstream crappy;
	crappy << "Errant memory after processing level " << threadMemoryConsumptionInMB - MemoryUsageAtInception << " in MB!\n";
	Logger::WriteLog(crappy.str());

	PListType earlyApproximation = file->fileString.size()/(256);

	for(unsigned int threadIteration = 0; threadIteration < testIterations; threadIteration = numThreads)
	{
		//Initialize all possible values for the first list to NULL
		for(int i = 0; i < 256*threadsToDispatch; i++)
		{
			prevPListArray->push_back(NULL);
			//prevPListArray->push_back(new vector<PListType>());

			/*localPrevPListArrayTriplet->push_back(new vector<vector<PListType>*>());
			for(int z = 0; z < numThreads; z++)
			{
				(*localPrevPListArrayTriplet)[i]->push_back(new vector<PListType>());
			}*/
		}


		memoryPerThread = memoryBandwidthMB/threadsToDispatch;
		cout << "Memory that can be used per thread: " << memoryPerThread << " MB." << endl;

		StopWatch time;

		StopWatch firstLevelTime;

		for(int z = 0; z < threadsToDispatch; z++)
		{
			currentLevelVector.push_back(0);
			activeThreads.push_back(false);
		}

		PListType overallFilePosition = 0;

		//make this value 1 so calculations work correctly then reset
		LevelPackage levelInfo;
		levelInfo.currLevel = 1;
		levelInfo.inceptionLevelLOL = 0;
		levelInfo.threadIndex = 0;
		levelInfo.useRAM = usedRAM[0];
		levelInfo.coreIndex = 0;

		bool prediction = PredictHardDiskOrRAMProcessing(levelInfo, 1);
		for(int i = 0; i < threadsToDispatch; i++)
		{
			usedRAM[i] = !prediction;
			currentLevelVector[i] = 1;
		}

		
		PListType fileReadSize = (PListType)((memoryPerThread*1000000)/3.0f);
		PListType fileIterations = file->fileStringSize/fileReadSize;
		if(file->fileStringSize%fileReadSize != 0)
		{
			fileIterations++;
		}

		//If using pure ram or prediction say we can use ram then we don't care about memory constraints
		if(usingPureRAM || !prediction)
		{
			fileIterations = 1;
		}

		vector<string> backupFilenames;

		file->copyBuffer->clear();
		file->copyBuffer->seekg(0, ios::beg);

		
		
		for(int z = 0; z < fileIterations; z++)
		{
			cout << "Number of threads processing file is " << threadsToDispatch << endl;
			PListType position = 0;
			PListType patternCount = 0;
			if(file->fileStringSize <= fileReadSize*z + fileReadSize)
			{
				patternCount = file->fileStringSize - fileReadSize*z;
			}
			else
			{
				patternCount = fileReadSize;
			}

			//load in the entire file
			if(usingPureRAM || !prediction)
			{
				patternCount = file->fileStringSize;
			}

			//new way to read in file
			file->fileString.clear();
			file->fileString.resize(patternCount);
			file->copyBuffer->read( &file->fileString[0], file->fileString.size());

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
					threadPlantSeedPoolHD->push_back(std::async(std::launch::any, &Forest::PlantTreeSeedThreadHD, this, overallFilePosition, position, span, i));
				#else
					threadPlantSeedPoolHD->push_back(std::async(launch::async /*| launch::deferred*/, &Forest::PlantTreeSeedThreadHD, this, overallFilePosition, position, span, i));
				#endif
				}
				else
				{
				#if defined(_WIN64) || defined(_WIN32)
					threadPlantSeedPoolRAM->push_back(std::async(std::launch::any, &Forest::PlantTreeSeedThreadRAM, this, overallFilePosition, position, span, i));
				#else
					threadPlantSeedPoolRAM->push_back(std::async(launch::async /*| launch::deferred*/, &Forest::PlantTreeSeedThreadRAM, this, overallFilePosition, position, span, i));
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



				/*int threadsFinished = 0;
				unsigned int threadsToDispatch = numThreads - 1;
				while (threadsFinished != threadsToDispatch)
				{
					for (unsigned int k = 0; k < threadsToDispatch; k++)
					{
						if (threadPlantSeedPoolRAM != NULL)
						{
							TreeRAM* temp = (*threadPlantSeedPoolRAM)[k].get();
							threadsFinished++;
						}
					}
				}*/
				
				firstLevelTime.Stop();
				crappy.str("");
				crappy << "Time taken to finish first level: ";
				Logger::WriteLog(crappy.str());
				cout << crappy.str() << endl;
				firstLevelTime.Display();


				//double totalStats = 0.0f;
				//double avgStats = 0.0f;
				//double stanDev = 0.0f;
				//cout << "File statistics..." << endl;
				//for (int i = 0; i < prevPListArray->size(); i++)
				//{
				//	statisticsModel.push_back((double)(*prevPListArray)[i]->size() / (double)file->fileStringSize);
				//	cout << "Byte: " << char(i) << " was observed in " << statisticsModel[i]*100.0f << " % of the file" << endl;
				//	totalStats += statisticsModel[i];
				//	
				//}
				//avgStats = totalStats / (double)prevPListArray->size();

				//for (int i = 0; i < prevPListArray->size(); i++)
				//{
				//	stanDev += powf(statisticsModel[i] - avgStats, 2.0f);
				//}
				//stanDev = stanDev / (double)prevPListArray->size();
				//stanDev = sqrtf(stanDev);
				//
				//cout << "Average: " << avgStats*100.0f << endl;
				//cout << "Standard Deviation: " << stanDev*100.0f << endl;

				if(levelRecordings.size() < levelInfo.currLevel)
				{
					levelRecordings.resize(levelInfo.currLevel);
				}
				levelRecordings[0] = 256;
				//if(mostCommonPattern.size() < levelInfo.currLevel)
				//{
				//	mostCommonPattern.resize(levelInfo.currLevel);
				//	mostCommonPatternCount.resize(levelInfo.currLevel);
				//}
				//
				//for (PListType i = 0; i < prevPListArray->size(); i++)
				//{
				//	if((*prevPListArray)[i] != nullptr)
				//	{
				//		
				//		levelRecordings[0]++;
				//		if( (*prevPListArray)[i]->size() > mostCommonPatternCount[levelInfo.currLevel - 1])
				//		{
				//			mostCommonPatternCount[levelInfo.currLevel - 1] = (*prevPListArray)[i]->size();
				//			//cout << (*(*prevPListArray)[i])[0] << endl;
				//			mostCommonPattern[levelInfo.currLevel - 1] = file->fileString.substr(((*(*prevPListArray)[i])[0] - (levelInfo.currLevel)), levelInfo.currLevel);
				//		}
				//	}
				//}
				//
				//if(coverage.size() < levelInfo.currLevel)
				//{
				//	coverage.resize(levelInfo.currLevel);
				//}
				//coverage[0] = ((float)(file->fileStringSize - (256 - levelRecordings[0])))/((float)file->fileStringSize);
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
			file->copyBuffer->clear();
			file->fileString.clear();
			file->fileString.reserve(0);
		}
		//Read in whole file if we are processing RAM potentially
		else
		{
			////new way to read in file
			//file->fileString.clear();
			//file->copyBuffer->seekg( 0 );
			//file->fileString.resize(file->fileStringSize);
			//file->copyBuffer->read( &file->fileString[0], file->fileString.size());
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
			}
		}
		else
		{
			//nothing for now
		}

		//return;

		DisplayPatternsFound();       

		StopWatch RestOfLevelTime;
		
		//Start searching
		if(2 <= maximum)
		{
			NextLevelTreeSearch(2);
		}

		RestOfLevelTime.Stop();
		crappy.str("");
		crappy << "Time taken to finish levels after first: ";
		Logger::WriteLog(crappy.str());
		RestOfLevelTime.Display();
			
		for(int j = 0; j < levelRecordings.size() && levelRecordings[j] != 0; j++)
		{
			
			stringstream buff;
			buff << "Level " << j + 1 << " count is " << levelRecordings[j] << /*" with most common pattern being: \"" << mostCommonPattern[j] << "\" occured " << mostCommonPatternCount[j] << " and coverage was " << coverage[j] << "%" <<*/ endl;
			Logger::WriteLog(buff.str());
			//cout << buff.str();
		}
		
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

		time.Stop();
		time.Display();
		cout << threadsToDispatch << " threads were used to process file" << endl;

		numThreads = (numThreads * 2) - 1;
		threadsToDispatch = numThreads - 1;
		
		//reset global level in case we are testing
		globalLevel = 1;

		crappy.str("");
		crappy << "File Size " << file->fileStringSize << " and eliminated patterns " << eradicatedPatterns << endl;
		Logger::WriteLog(crappy.str());

	}

	//Close file handle once and for all
	file->copyBuffer->clear();
	file->fileString.clear();
	file->fileString.reserve(0);

	
	if(memoryQueryThread != NULL)
	{
		processingFinished = true;
		memoryQueryThread->join();
		delete memoryQueryThread;
	}
	
	if(msyncThread != NULL)
	{
		processingMSYNCFinished = true;
		msyncThread->join();
		delete msyncThread;
	}

	for(int i = 0; i < 256; i++)
	{
		delete gatedMutexes[i];
	}
	delete file;
	
	delete threadPool;

	delete countMutex;
	delete prevPListArray;
	delete globalPListArray;
	delete localPrevPListArrayTriplet;
	delete localGlobalPListArrayTriplet;
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

void Forest::ProcessChunksIntoUnifiedArray(PListType startPatternIndex, PListType numPatternsToSearch)
{
	for (PListType i = startPatternIndex; i < numPatternsToSearch + startPatternIndex; i++)
	{
		PListType count = 0;
		for (PListType j = 0; j < (*localPrevPListArrayTriplet)[i]->size(); j++)
		{
			count += (*(*localPrevPListArrayTriplet)[i])[j]->size();
		}
		//Reserve is very critical in making this work ffast!!! so DO NOT REMOVE!!!!!!!!!!!!!!!!! - PJM
		(*prevPListArray)[i]->reserve(count);
		
		for (PListType k = 0; k < (*localPrevPListArrayTriplet)[i]->size(); k++)
		{
			(*prevPListArray)[i]->insert((*prevPListArray)[i]->end(), (*(*localPrevPListArrayTriplet)[i])[k]->begin(), (*(*localPrevPListArrayTriplet)[i])[k]->end());
			delete (*(*localPrevPListArrayTriplet)[i])[k];
		}
		delete (*localPrevPListArrayTriplet)[i];
	}
}

void Forest::MonitorMSYNCThreads()
{
	int prevIndex = 0;
	int currIndex = 0;
	while(!processingMSYNCFinished)
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

		if(swTimer.GetTime() > 10000.0f)
		{
			loggingIt.str("");
			swTimer.Start();
			loggingIt << "Thread level status...\n";
			for(int j = 0; j < currentLevelVector.size(); j++)
			{
				loggingIt << "Thread " << j << " is at level: " << currentLevelVector[j] << endl;
			}
			cout << loggingIt.str() << endl;
			Logger::WriteLog(loggingIt.str());
			loggingIt.str("");
			loggingIt << "Percentage of file processed is: " << (((double)eradicatedPatterns)/((double)file->fileStringSize))*100.0f;
			cout << loggingIt.str() << endl;
			Logger::WriteLog(loggingIt.str());
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
	FileReader tempHelpFile(READMEPATH);
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
			mostCommonPatternCount.resize(maximum);
			mostCommonPattern.resize(maximum);
			maxEnter = true;
			i++;
		}
		else if (arg.compare("-f") == 0)
		{
			// We know the next argument *should* be the filename
			tempFileName.append(argv[i + 1]);
			file = new FileReader(tempFileName);
			fileEnter = true;
			i++;
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
		else if(arg.compare("-start") == 0)
		{
			startingLevel = atoi(argv[i + 1]);
			i++;
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

	/*if(!usingPureHD && !usingPureRAM)
	{
		usingMemoryBandwidth = false;
	}*/

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

	//If max not specified then make the largest pattern the fileSize
	if (!maxEnter)
	{
		maximum = file->fileStringSize;
	}
	//If min not specified then make the smallest pattern of 0
	if (!minEnter)
	{
		minimum = 0;
	}
	//If numCores is not specified then we use number of threads supported cores plus the main thread
	if (!threadsEnter)
	{
		numThreads = concurentThreadsSupported;
	}

	int bestThreadCount = 0;
	double fastestTime = 1000000000.0f;
	testIterations = 1;
	if (findBestThreadNumber)
	{
		numThreads = 1;
		testIterations = concurentThreadsSupported - 1;
	}

	prevFileNameList.resize(numThreads - 1);
	newFileNameList.resize(numThreads - 1);
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
	if(potentialPatterns > file->fileStringSize - (levelInfo.currLevel - 1))
	{
		//Factor in eradicated patterns because those places no longer need to be checked in the file
		potentialPatterns = (file->fileStringSize - eradicatedPatterns) - (levelInfo.currLevel - 1);
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
	PListType pListIndexesLeft = (file->fileStringSize - eradicatedPatterns)*sizeof(PListType);
	PListType predictedMemoryForNextLevelMB = (PListType)((vectorSize + pListIndexesLeft + sizeOfTreeMap)/1000000.0f);

	//cout << "Predicted size for level " << levelInfo.currLevel << " is " << predictedMemoryForNextLevelMB << " MB" << endl;
	
	PListType previousLevelMemoryMB = 0;
	PListType predictedMemoryForLevelProcessing = 0;
		
	//if(levelInfo.useRAM)
	//{
	//	previousLevelMemoryMB = MemoryUtils::GetProgramMemoryConsumption() - MemoryUsageAtInception;
	//}
	//else
	//{
	//	//If patterns calculated previously then use that data
	//	if(sizeOfPrevPatternCount > 0 && levelInfo.currLevel > 1)
	//	{
	//		PListType sizeOfTreeMap = sizeof(char)*sizeOfPrevPatternCount + sizeof(TreeHD)*sizeOfPrevPatternCount;
	//		PListType prevTreeSizeMB = (vectorSize + pListIndexesLeft + sizeOfTreeMap)/1000000.0f;
	//		cout << "Previous Tree mapping for level " << levelInfo.currLevel - 1 << " is " << prevTreeSizeMB << " MB" << endl;
	//		previousLevelMemoryMB = prevTreeSizeMB;
	//	}
	//	//If not rely on predictions which can be crude because of possible pattern overlap
	//	else
	//	{
	//		previousLevelMemoryMB = sizeOfPreviousLevelMB;
	//	}
	//}

	previousLevelMemoryMB = (PListType)(MemoryUtils::GetProgramMemoryConsumption() - MemoryUsageAtInception);

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
		if(file->fileString.size() == 0)
		{
			//new way to read in file
			file->fileString.resize(file->fileStringSize);
			file->copyBuffer->read( &file->fileString[0], file->fileString.size());
		}
		
		stringstream stringbuilder;
		stringbuilder << "Using RAM! Total size for level " << levelInfo.currLevel << " processing is " << predictedMemoryForLevelProcessing << " MB" << endl;
		cout << stringbuilder.str();
		Logger::WriteLog(stringbuilder.str());
		
		return false;
	}
}

//bool Forest::PredictHardDiskOrRAMProcessing(LevelPackage levelInfo, PListType sizeOfPrevPatternCount)
//{
//	//Break early if memory usage is predetermined by command line arguments
//	if(usingPureRAM)
//	{
//		return false;
//	}
//	if(usingPureHD)
//	{
//		return true;
//	}
//	//main thread is a hardware thread so dispatch threads requested minus 1
//	PListType threadsToDispatch = numThreads - 1;
//
//	//predictedMemoryForLevelProcessing has to include memory loading in from previous level and memory for the next level
//	//First calculate the size of the file because that is the maximum number of pLists we can store minus the level
//	//Second calculate the size of Tree objects will be allocated based on the number of POTENTIAL PATTERNS...
//	//POTENTIAL PATTERNS equals the previous list times 256 possible byte values but this value can't exceed the file size minus the current level
//	PListType potentialPatterns = sizeOfPrevPatternCount*256;
//	//cout << "Level " << levelInfo.currLevel << " had " << sizeOfPrevPatternCount << " patterns!" << endl;
//
//	//cout << "Eradicated file indexes: " << eradicatedPatterns << endl;
//	if(potentialPatterns > file->fileStringSize - (levelInfo.currLevel - 1))
//	{
//		//Factor in eradicated patterns because those places no longer need to be checked in the file
//		potentialPatterns = (file->fileStringSize - eradicatedPatterns) - (levelInfo.currLevel - 1);
//		//cout << "Potential patterns has exceed file size so we downgrade to " << potentialPatterns << endl;
//	}
//
//	//cout << "Potential patterns for level " << levelInfo.currLevel << " is " << potentialPatterns << endl;
//
//	PListType previousLevelMemoryMB = 0;
//	PListType predictedMemoryForLevelProcessing = 0;
//	PListType sizeOfTreeMap = 0;
//	if(levelInfo.useRAM)
//	{
//		sizeOfTreeMap = (potentialPatterns + 1)*32 + (potentialPatterns)*(potentialPatterns + sizeof(char)) + potentialPatterns * 32;
//		previousLevelMemoryMB = MemoryUtils::GetProgramMemoryConsumption() - MemoryUsageAtInception;
//	}
//	else
//	{
//		sizeOfTreeMap = (potentialPatterns + 1)*32 + (potentialPatterns)*(potentialPatterns + sizeof(char)) + potentialPatterns * 32;
//		PListType sizeOfPrevTreeMap = (sizeOfPrevPatternCount + 1)*32 + (sizeOfPrevPatternCount)*(sizeOfPrevPatternCount + sizeof(char)) + sizeOfPrevPatternCount * 32;
//		previousLevelMemoryMB = sizeOfPrevTreeMap/1000000.0f;
//	}
//
//	PListType predictedMemoryForNextLevelMB = sizeOfTreeMap/1000000.0f;
//
//	//cout << "Size used for previous level " << levelInfo.currLevel - 1 << " is " << previousLevelMemoryMB << " MB" << endl;
//	
//	predictedMemoryForLevelProcessing = previousLevelMemoryMB + predictedMemoryForNextLevelMB;
//	if(predictedMemoryForLevelProcessing > memoryBandwidthMB)
//	{
//	
//		stringstream stringbuilder;
//		stringbuilder << "Using HARD DISK! Total size for level " << levelInfo.currLevel << " processing is " << predictedMemoryForLevelProcessing << " MB" << endl;
//		cout << stringbuilder.str();
//		Logger::WriteLog(stringbuilder.str());
//		
//		return true;
//	}
//	else
//	{
//		if(file->fileString.size() == 0)
//		{
//			//new way to read in file
//			file->fileString.resize(file->fileStringSize);
//			file->copyBuffer->read( &file->fileString[0], file->fileString.size());
//		}
//		
//		stringstream stringbuilder;
//		stringbuilder << "Using RAM! Total size for level " << levelInfo.currLevel << " processing is " << predictedMemoryForLevelProcessing << " MB" << endl;
//		cout << stringbuilder.str();
//		Logger::WriteLog(stringbuilder.str());
//		
//		return false;
//	}
//}

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
			for(PListType prevIndex = 0; prevIndex < prevLocalPListArray->size(); prevIndex++)
			{
				if((*prevLocalPListArray)[prevIndex] != NULL)
				{
					list<PListType> *sorting = new list<PListType>();
					copy( (*prevLocalPListArray)[prevIndex]->begin(), (*prevLocalPListArray)[prevIndex]->end(), std::back_inserter(*sorting));
					((*prevLocalPListArray)[prevIndex])->erase(((*prevLocalPListArray)[prevIndex])->begin(), ((*prevLocalPListArray)[prevIndex])->end());
					sorting->sort();
					std::copy( sorting->begin(), sorting->end(), std::back_inserter(*((*prevLocalPListArray)[prevIndex])));
					sorting->clear();
					delete sorting;

					threadFiles[threadNumber]->WriteArchiveMapMMAP(*(*prevLocalPListArray)[prevIndex]);
					delete (*prevLocalPListArray)[prevIndex];

					if(threadFiles[threadNumber]->totalWritten >= 262144) 
					{
						threadFiles[threadNumber]->WriteArchiveMapMMAP(vector<PListType>(), "", true);
					}
				
					//Increment chunk
					threadNumber++;
					if(threadNumber >= threadsToDispatch)
					{
						threadNumber = 0;
					}
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
		}
	}
	
	for(int a = 0; a < usedRAM.size(); a++)
	{
		usedRAM[a] = !prediction;
	}
	
	

	//countMutex->lock();
	//
	//int releaseGlobalString = 0;
	//for(PListType i = 0; i < threadsToDispatch; i++)
	//{
	//	if(!usedRAM[i])
	//	{
	//		releaseGlobalString++;
	//	}
	//}

	//if(releaseGlobalString == threadsToDispatch)
	//{
	//	file->fileString.clear();
	//	file->fileString.resize(0);
	//}

	//if(releaseGlobalString < threadsToDispatch && file->fileString.size() == 0)
	//{
	//	//new way to read in file
	//	file->fileString.clear();
	//	file->fileString.resize(file->fileStringSize);
	//	file->copyBuffer->read( &file->fileString[0], file->fileStringSize);
	//}

	//countMutex->unlock();
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

				if(threadFile->totalWritten >= 262144) 
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

	if(levelInfo.useRAM && file->fileString.size() != file->fileStringSize)
	{
		//new way to read in file
		file->copyBuffer->seekg( 0 );
		file->fileString.resize(file->fileStringSize);
		file->copyBuffer->read( &file->fileString[0], file->fileString.size());
	}
	
	//countMutex->lock();
	//
	//int releaseGlobalString = 0;
	//for(PListType i = 0; i < threadsToDispatch; i++)
	//{
	//	if(!usedRAM[i])
	//	{
	//		releaseGlobalString++;
	//	}
	//}

	//if(releaseGlobalString == threadsToDispatch && file->fileString.size() > 0)
	//{
	//	globalUsingRAM = false;
	//	file->fileString.clear();
	//	file->fileString.resize(0);
	//	
	//}

	//if(releaseGlobalString < threadsToDispatch && file->fileString.size() == 0)
	//{
	//	//new way to read in file
	//	globalUsingRAM = true;
	//	file->fileString.clear();
	//	file->fileString.resize(file->fileStringSize);
	//	file->copyBuffer->read( &file->fileString[0], file->fileStringSize);
	//	
	//}

	//countMutex->unlock();
	
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
		//vector<vector<PListType>> balancedTruncList = ProcessThreadsWorkLoadRAM(threadsToDispatch, prevPListArray);
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
			threadPool->push_back(std::async(std::launch::any, &Forest::ThreadedLevelTreeSearchRecursionList, this, prevPListArray, balancedTruncList[i], temp2, levelInfo));
		#else
			threadPool->push_back(std::async(launch::async /*| launch::deferred*/, &Forest::ThreadedLevelTreeSearchRecursionList, this, prevPListArray, balancedTruncList[i], temp2, levelInfo));
		#endif
			//threadPool->push_back(std::async(launch::async | launch::deferred, &Forest::ThreadedLevelTreeSearchRecursionList, this, prevPListArray, balancedTruncList[i], temp2, levelInfo));
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
			threadPool->push_back(std::async(std::launch::any, &Forest::ThreadedLevelTreeSearchRecursionList, this, prevPListArray, temp, balancedTruncList[i], levelInfo));
		#else
			threadPool->push_back(std::async(launch::async /*| launch::deferred*/, &Forest::ThreadedLevelTreeSearchRecursionList, this, prevPListArray, temp, balancedTruncList[i], levelInfo));
		#endif
			//threadPool->push_back(std::async(launch::async | launch::deferred, &Forest::ThreadedLevelTreeSearchRecursionList, this, prevPListArray, temp, balancedTruncList[i], levelInfo));
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
	try
	{
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
				if (/*localThreadPool != NULL && (*localThreadPool)[localWorkingThreads[k]].valid()*/ /*&& (*localThreadPool)[localWorkingThreads[k]]._Is_ready() && */(*localThreadPool)[localWorkingThreads[k]].wait_for(chrono::milliseconds(5)) == std::future_status::ready)
			#endif	
				{
					if(recursive)
					{
						(*localThreadPool)[localWorkingThreads[k]].get();

						/*stringstream buff;
						buff << "Recursive thread " << level << " finished all processing" << endl;
						Logger::WriteLog(buff.str());*/
						//cout << buff.str();
						
						threadsFinished++;
					}
					else
					{
						(*localThreadPool)[localWorkingThreads[k]].get();
						threadsFinished++;
						
						StopWatch time = initTime;
						time.Stop();
						time.Display();
						
						countMutex->lock();
						activeThreads[k] = false;
						countMutex->unlock();

						
						stringstream buff;
						buff << "Thread " << localWorkingThreads[k] << " finished all processing" << endl;
						Logger::WriteLog(buff.str());
						//cout << buff.str();

					}

				}
				else
				{
				//#if defined(_WIN64) || defined(_WIN32)
				//	//Wait 5 milliseconds
				//	this_thread::sleep_for(chrono::milliseconds(5));
				//#endif
					currentThreads.push_back(localWorkingThreads[k]);
				}
			}
			localWorkingThreads.clear();
			for(unsigned int i = 0; i < currentThreads.size(); i++)
			{
				localWorkingThreads.push_back(currentThreads[i]);
			}

			//Used to make sure the command prompt never idles to keep it alive on linux
			//Prints time spent every minute on program
			if(!recursive && oneSecondTimer.GetTime() > 60000.0f)
			{
				oneSecondTimer.Start();
				cout << "Finished threads: ";
				for(int j = 0; j < activeThreads.size(); j++)
				{
					if(!activeThreads[j])
					{
						cout << j << ", ";
					}
				}
				cout << endl;
				initTime.DisplayNow();
			}
		}
	}
	catch(exception e)
	{
		cout << e.what() << endl;
	}

	//Final assessment
	if(!recursive)
	{
		cout << "Finished threads: ";
		for(int j = 0; j < activeThreads.size(); j++)
		{
			if(!activeThreads[j])
			{
				cout << j << ", ";
			}
		}
		cout << endl;
		initTime.DisplayNow();
	}
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
		
		Logger::WriteLog("Not distributing files!");
		for(int a = 0; a < prevFileNames.size(); a++)
		{
			
			newFileList[threadNumber].push_back(prevFileNames[a]);
			
			stringstream stringbuilder;
			stringbuilder << "Original file being non distributed : " << newFileList[threadNumber][newFileList[threadNumber].size() - 1] << endl;
			Logger::WriteLog(stringbuilder.str());
			
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
		
		Logger::WriteLog("Distributing files!\n");
		
		for(unsigned int a = 0; a < threadsToDispatch; a++)
		{
			threadFilesNames.str("");
			fileIDMutex->lock();
			fileID++;
			threadFilesNames << "PListChunks" << fileID;
			fileIDMutex->unlock();

			threadFiles.push_back(new PListArchive(threadFilesNames.str(), true));
			newFileList[a].push_back(threadFilesNames.str());
			
			stringstream stringbuilder;
			stringbuilder << "New file being distributed : " << threadFilesNames.str() << " at level " << levelInfo.currLevel << " at inception " << levelInfo.inceptionLevelLOL << endl;
			Logger::WriteLog(stringbuilder.str());
			
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
	for(PListType i = 0; i < threadsToDispatch; i++)
	{
		
		/*if(i < localWorkingThreads.size())
		{
			cout << "Thread " << i << " is processing " << balancedSizeList[i] << " indexes with " << balancedTruncList[i].size() << " patterns\n";
		}
		else
		{
			cout << "Thread " << i << " is not processing " << balancedSizeList[i] << " indexes with " << balancedTruncList[i].size() << " patterns\n";
		}*/
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
			if((*patterns)[i*threadsToDispatch + j] != NULL)
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
				string tempString = archiveCollection[a]->fileName;
				tempString.erase(i, 8);
				tempString.erase(0, 7);
				
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
			/*	sizeDifference << "Actual patterns left to process: " << totalPatterns << " and the total patterns we can dump is: " << finalMetaDataMap.size() - patternsThatCantBeDumped.size() << endl;
				sizeDifference << "Size of Meta Data Map: " << finalMetaDataMap.size() << endl;
				
				if(totalPatterns != patternsToDumpCount)
				{
					sizeDifference << "We don't have a match!" << endl;
				}
				Logger::WriteLog(sizeDifference.str());*/

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

							if(mostCommonPattern.size() < currLevel)
							{
								mostCommonPattern.resize(currLevel);
								mostCommonPatternCount.resize(currLevel);
							}
				
							if(iterator->second->size() > mostCommonPatternCount[currLevel - 1])
							{
								mostCommonPatternCount[currLevel - 1] = iterator->second->size();
				
								mostCommonPattern[currLevel - 1] = iterator->first;
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

					//END OF ADDED CODE
				}
				foundAHit = false;

				std::string::size_type i = archiveCollection[a]->fileName.find(".txt");
				string tempString = archiveCollection[a]->fileName;
				tempString.erase(i, 8);
				tempString.erase(0, 7);

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
						
						//string pattern = (*stringBuffer)[partialLists];
							
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

				PListType newCount = packedPListSize - countAdded;
				if(newCount > 0)
				{
					string backupFile = fileNameForLater;
					std::string::size_type j = backupFile.find("_");
					backupFile.erase(j + 1, backupFile.size() - j);
					stringstream buffy;
					buffy << backupFile << newCount;
					fileNamesBackup[prevCurrentFile + a] = buffy.str();
					
					PListType testCount = 0;
					PListArchive* archiveCollective = new PListArchive(fileNamesBackup[prevCurrentFile + a], true);
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

				if(mostCommonPattern.size() < currLevel)
				{
					mostCommonPattern.resize(currLevel);
					mostCommonPatternCount.resize(currLevel);
				}
				
				if(iterator->second->size() > mostCommonPatternCount[currLevel - 1])
				{
					mostCommonPatternCount[currLevel - 1] = iterator->second->size();
				
					mostCommonPattern[currLevel - 1] = iterator->first;
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
				DeleteChunk(newFileNames[newFileNames.size() - 1], ARCHIVE_FOLDER);
				newFileNames.pop_back();
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
	coverage[currLevel - 1] += ((float)(interimCount))/((float)file->fileStringSize);
	
	stringstream buffy;
	buffy << currLevel << " with a total of " << levelRecordings[currLevel - 1] << " using HD" << endl;
	Logger::WriteLog(buffy.str());

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
				//memoryOverflow = true;
			}
			else if(overMemoryCount && memoryOverflow)
			{
				stringstream crap;
				crap << "Overflow at Process Chunks And Generate of " << currMemoryOverflow << " in MB!\n";
				Logger::WriteLog(crap.str());
				//wait for one second for other memory to clear up
				//std::this_thread::sleep_for (std::chrono::seconds(1));
			}

			
			archiveCollection[a]->GetPListArchiveMMAP(packedPListArray); //Needs MB
			packedPListSize = packedPListArray.size();

			std::string::size_type i = archiveCollection[a]->fileName.find(".txt");
			string tempString = archiveCollection[a]->fileName;
			tempString.erase(i, 8);
			tempString.erase(0, 7);
				
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
							//memoryOverflow = true;
						}

						string pattern = (*stringBuffer)[partialLists];
						
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

			/*for(int a = 0; a < currPatternCount; a++)
			{
				stringstream pattern;
				pattern << (char)a;
				currChunkFiles[pattern.str()]->WriteArchiveMapMMAP(vector<PListType>(), "", true);
			}*/


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
			currChunkFiles[buff.str()]->WriteArchiveMapMMAP(vector<PListType>(), "", true/*, true*/);
			bool empty = true;
			PListType patterCount = (currChunkFiles[buff.str()]->prevMappingIndex/sizeof(PListType)) - sizeof(PListType);
			if(currChunkFiles[buff.str()]->mappingIndex > 0 /*|| (outlierScans && patterCount == 1)*/)
			{
				empty = false;
				interimCount++;

				PListType patterCount = (currChunkFiles[buff.str()]->prevMappingIndex/sizeof(PListType)) - sizeof(PListType);
				if(patterCount > mostCommonPatternCount[currLevel - 1])
				{
					mostCommonPatternCount[currLevel - 1] = patterCount;
				
					mostCommonPattern[currLevel - 1] = buff.str();
				}
			}
			string fileToDelete = currChunkFiles[buff.str()]->patternName;
			currChunkFiles[buff.str()]->CloseArchiveMMAP();
			delete currChunkFiles[buff.str()];

			if(fileNamesBackup.size() == currentFile && empty)
			{
				DeleteChunk(fileToDelete, ARCHIVE_FOLDER);
			}

			if(mostCommonPattern.size() < currLevel)
			{
				mostCommonPattern.resize(currLevel);
				mostCommonPatternCount.resize(currLevel);
			}
				
			
		}
		
		countMutex->lock();
		eradicatedPatterns += 256 - interimCount;
		countMutex->unlock();

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
	coverage[currLevel - 1] += ((float)(interimCount))/((float)file->fileStringSize);
	
	stringstream buffy;
	buffy << currLevel << " with a total of " << levelRecordings[currLevel - 1] << endl;
	Logger::WriteLog(buffy.str());

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
	PListType memDivisor = (PListType)((memoryPerThread*1000000)/3.0f);
		
	bool morePatternsToFind = false;

	unsigned int fileIters = (unsigned int)(file->fileStringSize/memDivisor);
	if(file->fileStringSize%memDivisor != 0)
	{
		fileIters++;
	}

	//string fileChunkData;
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
						
							//if((*(*packedPListArray)[m])[(*packedPListArray)[m]->size() - 1] > maximum)
							//{
							//	//Add level info because that can mean we need overlap of next chunk hehe got ya!
							//	maximum = ((*(*packedPListArray)[m])[(*packedPListArray)[m]->size() - 1] + currLevel);
							//}
						}
					}

					unsigned int firstIndex = (unsigned int)(minimum/memDivisor);
					unsigned int lastIndex = fileIters;
					/*int lastIndex = maximum/memDivisor;
					if(lastIndex == fileIters - 1 && maximum%memDivisor != 0)
					{
						lastIndex = fileIters;
					}*/

					int threadChunkToUse = threadNum;
					//for(int j = 0; j < fileIters; j++)
					for(unsigned int j = firstIndex; j < lastIndex && minimum != -1; j++)
					{
					

						if(packedListSize > 0)
						{
							if(fileChunks.size() > threadChunkToUse && fileChunks[threadChunkToUse].size() > 0)
							{
								saveOffPreviousStringData = fileChunks[threadChunkToUse].substr(fileChunks[threadChunkToUse].size() - (currLevel - 1), currLevel - 1);
							}

							PListType patternCount = 0;
							if(file->fileStringSize <= memDivisor*j + memDivisor)
							{
								patternCount = file->fileStringSize - memDivisor*j;
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
							/*for(int bam = 0; bam < chunkOrigin.size(); bam++)
							{
								if(chunkOrigin[bam] == j)
								{
									threadChunkToUse = bam;
									foundChunkInUse = true;
									break;
								}
							}*/

							if(!foundChunkInUse)
							{
								////When another thread is done using my original threads data then we can release it and get new data
								//bool released = false;
								//while(!released)
								//{
								//	released = true;
								//	for(int bam = 0; bam < chunksBeingUsed.size(); bam++)
								//	{
								//		if(chunksBeingUsed[bam] == threadNum)
								//		{
								//			released = false;
								//		}
								//	}
								//}

								/*if(fileChunks[threadChunkToUse].size() != 0)
								{
									for(int bam = 0; bam < chunkOrigin.size(); bam++)
									{
										if(chunkOrigin[bam] == -1)
										{
											threadChunkToUse = bam;
											break;
										}
									}
								}*/
								writingFlag = true;

								//Shared file space test
								fileChunks.push_back("");
								fileChunks[fileChunks.size() - 1].resize(patternCount);
							
								PListType offset = memDivisor*j;
								FileReader fileReaderTemp(file->fileName);
								fileReaderTemp.copyBuffer->seekg( offset );
								fileReaderTemp.copyBuffer->read( &fileChunks[fileChunks.size() - 1][0], patternCount );
							
								//chunkOrigin[threadNum] = j;

								threadChunkToUse = (int)(fileChunks.size() - 1);

								chunkIndexToFileChunk[j] = threadChunkToUse;

								writingFlag = false;
							
							}
							else
							{
								//otherwise use what has already been lifted from file
							}
							//Label if thread is using its own data or data from another thread
							//chunksBeingUsed[threadNum] = threadChunkToUse;
							//Now that we have grabbed the file data we say this thread is using it
							//usingFileChunk[threadNum] = j;

						
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

						
							//if(!overMemoryCount)
							//if(sizeInMB < memDivisor/1000000.0f)
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
											if(((*pList)[k]) < file->fileStringSize)
											{
												//If index comes out to be larger than fileString than it is a negative number 
												//and we must use previous string data!
												if(((((*pList)[k]) - memDivisor*j) - (currLevel-1)) >= file->fileStringSize)
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
											
													//cout << "string over the border: " << saveOffPreviousStringData << endl;
													//cout << "Relative index: " << relativeIndex << " Index for string: " << indexForString << " pattern: " << pattern << " is size: " << pattern.size() << endl;
												}
												else
												{
													//If pattern is past end of string stream then stop counting this pattern
													if(((*pList)[k]) < file->fileStringSize)
													{
												
														string pattern = fileChunks[threadChunkToUse].substr(((((*pList)[k]) - memDivisor*j) - (currLevel-1)), currLevel);
													
														if(patternToSearchFor.size() == 0 || pattern[pattern.size() - 1] == patternToSearchFor[levelInfo.currLevel - 1])
														{
															/*if(leaf.leaves.find(pattern) != leaf.leaves.end())
															{
																globalTotalMemoryInBytes -= leaf.leaves[pattern].pList.capacity()*sizeof(PListType);
															}*/
															leaf.addLeaf((*pList)[k]+1, pattern);
															globalTotalMemoryInBytes += sizeof(PListType);
														}
													
													}
													else if(((((*pList)[k]) - memDivisor*j) - (currLevel-1)) < 0)
													{
														cout << "String access is out of bounds at beginning" << endl;
													}
													else if((((*pList)[k]) - memDivisor*j) >= file->fileStringSize)
													{
														cout << "String access is out of bounds at end" << endl;
													}
												}

											}
											else
											{
												cout << "don't pattern bro at this index: " << ((*pList)[k]) << endl;
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
									fileNamesToReOpen.push_back(CreateChunkFile(stringBuilder.str(), leaf, levelInfo/*threadNum, currLevel,*/ ));
								
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
							fileNamesToReOpen.push_back(CreateChunkFile(stringBuilder.str(), leaf, levelInfo/*threadNum, currLevel*/));

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
	}
	catch(exception e)
	{
		cout << e.what() << endl;
		MemoryUtils::print_trace();
	}

	//usingFileChunk[threadNum] = -1;

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
		/*countMutex->lock();
		availableCores.push_back(levelInfo.coreIndex);
		countMutex->unlock();*/
		DeleteArchives(fileList, ARCHIVE_FOLDER);
	}
	return morePatternsToFind;
}

bool Forest::DispatchNewThreadsRAM(PListType newPatternCount, bool& morePatternsToFind, vector<vector<PListType>*>* prevLocalPListArray, LevelPackage levelInfo, bool& isThreadDefuncted)
{
	bool dispatchedNewThreads = false;
	//bool alreadyUnlocked = false;
	//countMutex->lock();

	//int threadsToDispatch = numThreads - 1;
	//int unusedCores = (threadsToDispatch - (threadsDispatched - threadsDefuncted)) + 1;
	//if(prevLocalPListArray->size() < unusedCores && unusedCores > 1)
	//{
	//	unusedCores = (int)prevLocalPListArray->size();
	//}
	////Need to have an available core, need to still have patterns to search and need to have more than 1 pattern to be worth splitting up the work
	//if(unusedCores > 1 && morePatternsToFind && prevLocalPListArray->size() > 1)
	//{
	//	//OLD WAY THREAD PRORITY//
	//	/*unsigned int levelCount = 1000000000;
	//	vector<int> threadPriority;

	//	for(int z = 0; z < currentLevelVector.size(); z++)
	//	{
	//		if(activeThreads[z])
	//		{
	//			if(currentLevelVector[z] < levelCount && activeThreads[z])
	//			{
	//				levelCount = currentLevelVector[z];
	//				threadPriority.push_back(z);
	//			}
	//		}
	//	}

	//	bool spawnThreads = false;
	//	for(int z = 0; z < threadPriority.size(); z++)
	//	{
	//		if(threadPriority[z] == levelInfo.threadIndex && currentLevelVector[threadPriority[z]] == levelCount)
	//		{
	//			spawnThreads = true;
	//		}
	//	}*/
	//	//END OF OLD WAY THREAD PRORITY//
	//	bool spawnThreads = true;
	//	//If this thread is at the lowest level of progress spawn new threads
	//	if(spawnThreads)
	//	{
	//		
	//		vector<vector<PListType>> balancedTruncList = ProcessThreadsWorkLoadRAM(unusedCores, prevLocalPListArray);
	//		vector<unsigned int> localWorkingThreads;
	//		for(unsigned int i = 0; i < balancedTruncList.size(); i++)
	//		{
	//			localWorkingThreads.push_back(i);
	//		}

	//		if(localWorkingThreads.size() > 1)
	//		{
	//			int threadsToTest = (threadsDispatched - threadsDefuncted) - 1;
	//			if(threadsToTest + localWorkingThreads.size() <= threadsToDispatch)
	//			{
	//					
	//				for(int z = 0; z < balancedTruncList.size(); z++)
	//				{
	//					unsigned int tally = 0;
	//					for(int d = 0; d < balancedTruncList[z].size(); d++)
	//					{
	//						tally += (unsigned int)(*prevLocalPListArray)[balancedTruncList[z][d]]->size();
	//					}
	//				}

	//				dispatchedNewThreads = true;
	//				//cout << "Thread " << levelInfo.threadIndex << " has priority and is at level " << levelInfo.currLevel << endl;

	//				LevelPackage levelInfoRecursion;
	//				levelInfoRecursion.currLevel = levelInfo.currLevel;
	//				levelInfoRecursion.threadIndex = levelInfo.threadIndex;
	//				levelInfoRecursion.inceptionLevelLOL = levelInfo.inceptionLevelLOL + 1;
	//				levelInfoRecursion.useRAM = true;

	//				//cout << "Current threads in use: " << threadsDispatched - threadsDefuncted + localWorkingThreads.size() - 1 << endl;

	//				threadsDefuncted++;
	//				isThreadDefuncted = true;

	//				vector<future<void>> *localThreadPool = new vector<future<void>>();
	//				for (PListType i = 0; i < localWorkingThreads.size(); i++)
	//				{
	//					threadsDispatched++;
	//					//levelInfoRecursion.coreIndex = availableCores[i];
	//				#if defined(_WIN64) || defined(_WIN32)
	//					localThreadPool->push_back(std::async(std::launch::any, &Forest::ThreadedLevelTreeSearchRecursionList, this, prevLocalPListArray, balancedTruncList[i], vector<string>(), levelInfoRecursion));
	//				#else
	//					localThreadPool->push_back(std::async(launch::async /*| launch::deferred*/, &Forest::ThreadedLevelTreeSearchRecursionList, this, prevLocalPListArray, balancedTruncList[i], vector<string>(), levelInfoRecursion));
	//				#endif
	//				}
	//				countMutex->unlock();
	//					
	//				alreadyUnlocked = true;
	//				WaitForThreads(localWorkingThreads, localThreadPool, true, levelInfo.threadIndex);

	//				localThreadPool->erase(localThreadPool->begin(), localThreadPool->end());
	//				(*localThreadPool).clear();
	//				delete localThreadPool;
	//				morePatternsToFind = false;
	//			}
	//		}
	//	}
	//	else
	//	{
	//			
	//	}
	//}
	//if(!alreadyUnlocked)
	//{
	//	countMutex->unlock();
	//}
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
					//cout << "Thread " << levelInfo.threadIndex << " has priority and is at level " << levelInfo.currLevel << endl;

					LevelPackage levelInfoRecursion;
					levelInfoRecursion.currLevel = levelInfo.currLevel;
					levelInfoRecursion.threadIndex = levelInfo.threadIndex;
					levelInfoRecursion.inceptionLevelLOL = levelInfo.inceptionLevelLOL + 1;
					levelInfoRecursion.useRAM = false;

					//cout << "Current threads in use: " << threadsDispatched - threadsDefuncted + localWorkingThreads.size() - 1 << endl;

					threadsDefuncted++;
					isThreadDefuncted = true;

					availableCores.push_back(levelInfo.coreIndex);
						
					vector<future<void>> *localThreadPool = new vector<future<void>>();
					for (PListType i = 0; i < localWorkingThreads.size(); i++)
					{
						threadsDispatched++;

						/*levelInfoRecursion.coreIndex = availableCores[availableCores.size() - 1];
						availableCores.pop_back();*/

						vector<PListType> temp;
					#if defined(_WIN64) || defined(_WIN32)
						localThreadPool->push_back(std::async(std::launch::any, &Forest::ThreadedLevelTreeSearchRecursionList, this, prevPListArray, temp, balancedTruncList[i], levelInfoRecursion));
					#else
						localThreadPool->push_back(std::async(launch::async /*| launch::deferred*/, &Forest::ThreadedLevelTreeSearchRecursionList, this, prevPListArray, temp, balancedTruncList[i], levelInfoRecursion));
					#endif
						//localThreadPool->push_back(std::async(launch::async | launch::deferred, &Forest::ThreadedLevelTreeSearchRecursionList, this, prevPListArray, temp, balancedTruncList[i], levelInfoRecursion));
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


//bool Forest::ProcessRAM(vector<vector<PListType>*>* prevLocalPListArray, vector<vector<PListType>*>* globalLocalPListArray, LevelPackage& levelInfo, bool &isThreadDefuncted)
//{
//	bool continueSearching = true;
//	int threadsToDispatch = numThreads - 1;
//	PListType totalTallyRemovedPatterns = 0;
//	
//	if(levelInfo.currLevel == 2)
//	{
//		PListType hey = 256/threadsToDispatch;
//		PListType startIndex = levelInfo.threadIndex*hey;
//		//cout << "Start index: " << startIndex << endl;
//		PListType earlyApproximation = (file->fileString.size() - eradicatedPatterns)/(prevLocalPListArray->size()*256*(numThreads - 1));
//		vector<PListType> newPList[256];
//		PListType prevPListSize = prevLocalPListArray->size();
//		PListType indexes[256] = {0};
//		PListType indexesToPush[256] = {0};
//		//Banking off very random patterns
//		PListType firstPatternIndex[256] = {0};
//		uint8_t prevPattern = 0;
//		int listLength = 0;
//
//		for (PListType i = 0; i < prevPListSize; i++)
//		{
//			vector<PListType>* pList = (*prevLocalPListArray)[i];
//			PListType pListLength = (*prevLocalPListArray)[i]->size();
//			if((*prevLocalPListArray)[i]->size() > 0)
//			{
//				prevPattern = (uint8_t)file->fileString[(*pList)[0] - 1];
//			}
//			
//			if(pListLength > 1)
//			{		
//				for (PListType k = 0; k < pListLength; k++)
//				{
//					//If pattern is past end of string stream then stop counting this pattern
//					PListType index = (*pList)[k];
//					if (index < file->fileStringSize)
//					{
//
//						uint_fast8_t indexIntoFile = (uint8_t)file->fileString[index];
//
//						if(firstPatternIndex[indexIntoFile])
//						{
//							if(newPList[indexIntoFile].empty())
//							{
//								newPList[indexIntoFile].push_back(firstPatternIndex[indexIntoFile]);	
//							}
//							newPList[indexIntoFile].push_back(index + 1);	
//							indexes[indexIntoFile]++;
//						}
//						else
//						{
//							firstPatternIndex[indexIntoFile] = index + 1;
//							indexes[indexIntoFile]++;
//							indexesToPush[listLength++] = indexIntoFile;
//						}
//					}
//					else
//					{
//						totalTallyRemovedPatterns++;
//					}
//				}
//			}
//			else
//			{
//				totalTallyRemovedPatterns++;
//			}
//
//			vector<PListType>* pList2 = NULL;
//			uint8_t newPattern = 0;
//			if(i != prevPListSize - 1 && (*prevLocalPListArray)[i+1]->size() != 0)
//			{
//				pList2 = (*prevLocalPListArray)[i+1];
//				newPattern = (uint8_t)file->fileString[(*pList2)[0] - 1];
//			}
//			if(i == prevPListSize - 1 || prevPattern != newPattern)
//			{
//				for (PListType k = 0; k < listLength; k++)
//				{
//					int insert = indexes[indexesToPush[k]];
//					if (insert >= minOccurrence)
//					{
//						int index = globalLocalPListArray->size();
//
//						globalLocalPListArray->push_back(new vector<PListType>(newPList[indexesToPush[k]]));
//						//(*globalLocalPListArray)[index]->insert((*globalLocalPListArray)[index]->end(), newPList[indexesToPush[k]].begin(), newPList[indexesToPush[k]].end());
//						indexes[indexesToPush[k]] = 0;
//						firstPatternIndex[indexesToPush[k]] = 0;
//						newPList[indexesToPush[k]].clear();
//					}
//					else if(insert == 1)
//					{
//						totalTallyRemovedPatterns++;
//						indexes[indexesToPush[k]] = 0;
//						firstPatternIndex[indexesToPush[k]] = 0;
//						newPList[indexesToPush[k]].clear();
//					}
//						
//				}
//					
//				listLength = 0;
//			}
//			delete (*prevLocalPListArray)[i];
//		}
//	}
//	else
//	{
//	
//		PListType earlyApproximation = (file->fileString.size() - eradicatedPatterns)/(prevLocalPListArray->size()*256*(numThreads - 1));
//		vector<PListType> newPList[256];
//		PListType prevPListSize = prevLocalPListArray->size();
//		PListType indexes[256] = {0};
//		PListType indexesToPush[256] = {0};
//		//Banking off very random patterns
//		register PListType firstPatternIndex[256] = {0};
//		PListType preloadedFirstPatternIndexes[20] = {0};
//		for (PListType i = 0; i < prevPListSize; i++)
//		{
//			vector<PListType>* pList = (*prevLocalPListArray)[i];
//			PListType pListLength = (*prevLocalPListArray)[i]->size();
//			if(pListLength > 1)
//			{
//				int listLength = 0;
//
//
//				for (PListType k = 0; k < pListLength; k++)
//				{
//					
//					////If pattern is past end of string stream then stop counting this pattern
//					//if(k % 20 == 0 && pListLength >= 20 && k < pListLength - 20)
//					//{
//					//	PListType indexTemp;
//					//	uint_fast8_t indexIntoFileTemp;
//					//	PListType firstPatternIndexValTemp;
//					//	for(int z = 0; z < 20; z++)
//					//	{
//					//		indexTemp = (*pList)[k+z];
//					//		if (indexTemp < file->fileStringSize)
//					//		{
//					//			indexIntoFileTemp = (uint8_t)file->fileString[indexTemp];
//					//			preloadedFirstPatternIndexes[z] = firstPatternIndex[indexIntoFileTemp];
//					//		}
//					//	}
//					//}
//					//else if(k % 20 == 0 && pListLength < 20 && k < pListLength - 20)
//					//{
//					//	PListType indexTemp;
//					//	uint_fast8_t indexIntoFileTemp;
//					//	PListType firstPatternIndexValTemp;
//					//	for(int z = 0; z < pListLength; z++)
//					//	{
//					//		indexTemp = (*pList)[k+z];
//					//		if (indexTemp < file->fileStringSize)
//					//		{
//					//			indexIntoFileTemp = (uint8_t)file->fileString[indexTemp];
//					//			preloadedFirstPatternIndexes[z] = firstPatternIndex[indexIntoFileTemp];
//					//		}
//					//	}
//					//}
//
//
//				/*	if(k%20 == 0)
//					{
//						PListType* index = pList->data() + k;
//						_mm_prefetch((const char *)index, _MM_HINT_T0);
//					}*/
//
//					PListType index = (*pList)[k];
//					
//					if (index < file->fileStringSize)
//					{
//						uint_fast8_t indexIntoFile = (uint8_t)file->fileString[index];
//						
//						if(firstPatternIndex[indexIntoFile])
//						{
//							if(newPList[indexIntoFile].empty())
//							{
//								newPList[indexIntoFile].push_back(firstPatternIndex[indexIntoFile]);	
//							}
//							newPList[indexIntoFile].push_back(index + 1);	
//							indexes[indexIntoFile]++;
//						}
//						else
//						{
//							firstPatternIndex[indexIntoFile] = index + 1;
//							//preloadedFirstPatternIndexes[k%20] = index + 1;
//							indexes[indexIntoFile]++;
//							indexesToPush[listLength++] = indexIntoFile;
//						}
//					}
//					else
//					{
//						totalTallyRemovedPatterns++;
//					}
//				}
//			
//				for (PListType k = 0; k < listLength; k++)
//				{
//					int insert = indexes[indexesToPush[k]];
//					if (insert >= minOccurrence)
//					{
//						int index = globalLocalPListArray->size();
//
//						globalLocalPListArray->push_back(new vector<PListType>(newPList[indexesToPush[k]]));
//						//(*globalLocalPListArray)[index]->insert((*globalLocalPListArray)[index]->end(), newPList[indexesToPush[k]].begin(), newPList[indexesToPush[k]].end());
//						indexes[indexesToPush[k]] = 0;
//						firstPatternIndex[indexesToPush[k]] = 0;
//						newPList[indexesToPush[k]].clear();
//					}
//					else if(insert == 1)
//					{
//						totalTallyRemovedPatterns++;
//						indexes[indexesToPush[k]] = 0;
//						firstPatternIndex[indexesToPush[k]] = 0;
//						newPList[indexesToPush[k]].clear();
//					}
//			
//				}
//				//delete pList;
//			}
//			else
//			{
//				//delete pList;
//				totalTallyRemovedPatterns++;
//			}
//			//delete pList;
//			delete (*prevLocalPListArray)[i];
//		}
//	}
//	
//
//	countMutex->lock();
//
//	eradicatedPatterns += totalTallyRemovedPatterns;
//
//	/*if(mostCommonPattern.size() < levelInfo.currLevel)
//	{
//		mostCommonPattern.resize(levelInfo.currLevel);
//		mostCommonPatternCount.resize(levelInfo.currLevel);
//	}
//	for (PListType i = 0; i < globalLocalPListArray->size(); i++)
//	{
//		if((*globalLocalPListArray)[i]->size() > mostCommonPatternCount[levelInfo.currLevel - 1])
//		{
//			mostCommonPatternCount[levelInfo.currLevel - 1] = (*globalLocalPListArray)[i]->size();
//				
//			mostCommonPattern[levelInfo.currLevel - 1] = file->fileString.substr(((*(*globalLocalPListArray)[i])[0] - (levelInfo.currLevel)), levelInfo.currLevel);
//		}
//	}*/
//		
//	if(levelRecordings.size() < levelInfo.currLevel)
//	{
//		levelRecordings.resize(levelInfo.currLevel);
//	}
//	levelRecordings[levelInfo.currLevel - 1] += globalLocalPListArray->size();
//
//	/*if(coverage.size() < levelInfo.currLevel)
//	{
//		coverage.resize(levelInfo.currLevel);
//	}
//	coverage[levelInfo.currLevel - 1] += ((float)(globalLocalPListArray->size()))/((float)file->fileStringSize);*/
//		
//	levelInfo.currLevel++;
//
//	if(levelInfo.currLevel > currentLevelVector[levelInfo.threadIndex])
//	{
//		currentLevelVector[levelInfo.threadIndex] = levelInfo.currLevel;
//	}
//	countMutex->unlock();
//
//	if(globalLocalPListArray->size() == 0 || levelInfo.currLevel - 1 >= maximum)
//	{
//		prevLocalPListArray->clear();
//		prevLocalPListArray->reserve(globalLocalPListArray->size());
//		prevLocalPListArray->swap((*globalLocalPListArray));
//		globalLocalPListArray->reserve(0);
//		continueSearching = false;
//		/*countMutex->lock();
//		availableCores.push_back(levelInfo.coreIndex);
//		countMutex->unlock();*/
//	}
//	else
//	{
//		prevLocalPListArray->clear();
//		prevLocalPListArray->reserve(globalLocalPListArray->size());
//		prevLocalPListArray->swap((*globalLocalPListArray));
//		globalLocalPListArray->reserve(0);
//		continueSearching = true;
//		DispatchNewThreadsRAM(globalLocalPListArray->size(), continueSearching, prevLocalPListArray, levelInfo, isThreadDefuncted);
//	}
//		
//	return continueSearching;
//}


//bool Forest::ProcessRAM(vector<vector<PListType>*>* prevLocalPListArray, vector<vector<PListType>*>* globalLocalPListArray, LevelPackage& levelInfo, bool &isThreadDefuncted)
//{
//	bool continueSearching = true;
//	int threadsToDispatch = numThreads - 1;
//	PListType totalTallyRemovedPatterns = 0;
//	
//	if(levelInfo.currLevel == 2)
//	{
//		PListType hey = 256/threadsToDispatch;
//		PListType startIndex = levelInfo.threadIndex*hey;
//		//cout << "Start index: " << startIndex << endl;
//		PListType earlyApproximation = (file->fileString.size() - eradicatedPatterns)/(prevLocalPListArray->size()*256*(numThreads - 1));
//		vector<PListType> newPList[256];
//		PListType prevPListSize = prevLocalPListArray->size();
//		PListType indexes[256] = {0};
//		PListType indexesToPush[256] = {0};
//		//Banking off very random patterns
//		PListType firstPatternIndex[256] = {0};
//		uint8_t prevPattern = 0;
//		int listLength = 0;
//
//		for (PListType i = 0; i < prevPListSize; i++)
//		{
//			vector<PListType>* pList = (*prevLocalPListArray)[i];
//			PListType pListLength = (*prevLocalPListArray)[i]->size();
//			if((*prevLocalPListArray)[i]->size() > 0)
//			{
//				prevPattern = (uint8_t)file->fileString[(*pList)[0] - 1];
//			}
//			
//			if(pListLength > 1)
//			{		
//				for (PListType k = 0; k < pListLength; k++)
//				{
//					//If pattern is past end of string stream then stop counting this pattern
//					PListType index = (*pList)[k];
//					if (index < file->fileStringSize)
//					{
//
//						uint_fast8_t indexIntoFile = (uint8_t)file->fileString[index];
//
//						if(firstPatternIndex[indexIntoFile])
//						{
//							if(newPList[indexIntoFile].empty())
//							{
//								newPList[indexIntoFile].push_back(firstPatternIndex[indexIntoFile]);	
//							}
//							newPList[indexIntoFile].push_back(index + 1);	
//							indexes[indexIntoFile]++;
//						}
//						else
//						{
//							firstPatternIndex[indexIntoFile] = index + 1;
//							indexes[indexIntoFile]++;
//							indexesToPush[listLength++] = indexIntoFile;
//						}
//					}
//					else
//					{
//						totalTallyRemovedPatterns++;
//					}
//				}
//			}
//			else
//			{
//				totalTallyRemovedPatterns++;
//			}
//
//			vector<PListType>* pList2 = NULL;
//			uint8_t newPattern = 0;
//			if(i != prevPListSize - 1 && (*prevLocalPListArray)[i+1]->size() != 0)
//			{
//				pList2 = (*prevLocalPListArray)[i+1];
//				newPattern = (uint8_t)file->fileString[(*pList2)[0] - 1];
//			}
//			if(i == prevPListSize - 1 || prevPattern != newPattern)
//			{
//				for (PListType k = 0; k < listLength; k++)
//				{
//					int insert = indexes[indexesToPush[k]];
//					if (insert >= minOccurrence)
//					{
//						int index = globalLocalPListArray->size();
//
//						globalLocalPListArray->push_back(new vector<PListType>(newPList[indexesToPush[k]]));
//						//(*globalLocalPListArray)[index]->insert((*globalLocalPListArray)[index]->end(), newPList[indexesToPush[k]].begin(), newPList[indexesToPush[k]].end());
//						indexes[indexesToPush[k]] = 0;
//						firstPatternIndex[indexesToPush[k]] = 0;
//						newPList[indexesToPush[k]].clear();
//					}
//					else if(insert == 1)
//					{
//						totalTallyRemovedPatterns++;
//						indexes[indexesToPush[k]] = 0;
//						firstPatternIndex[indexesToPush[k]] = 0;
//						newPList[indexesToPush[k]].clear();
//					}
//						
//				}
//					
//				listLength = 0;
//			}
//			delete (*prevLocalPListArray)[i];
//		}
//	}
//	else
//	{
//	
//		PListType earlyApproximation = (file->fileString.size() - eradicatedPatterns)/(prevLocalPListArray->size()*256*(numThreads - 1));
//		vector<PListType> newPList[256];
//		PListType prevPListSize = prevLocalPListArray->size();
//		PListType indexes[256] = {0};
//		PListType indexesToPush[256] = {0};
//		//Banking off very random patterns
//		PListType firstPatternIndex[256] = {0};
//		
//		for (PListType i = 0; i < prevPListSize; i++)
//		{
//			vector<PListType>* pList = (*prevLocalPListArray)[i];
//			PListType pListLength = (*prevLocalPListArray)[i]->size();
//
//			vector<pair<PListType, uint8_t>> patternCombine;
//			patternCombine.reserve(pListLength);
//
//			if(pListLength > 1)
//			{
//				int listLength = 0;
//
//				for (PListType k = 0; k < pListLength; k++)
//				{
//					PListType index = (*pList)[k];
//					
//					if (index < file->fileStringSize)
//					{
//						uint8_t indexIntoFile = (uint8_t)file->fileString[index];
//						patternCombine.push_back( pair<PListType, int8_t>(index + 1, indexIntoFile) );
//					}
//					else
//					{
//						totalTallyRemovedPatterns++;
//					}
//				}
//
//				PListType patternCombineSize = patternCombine.size();
//				if(patternCombineSize > 2)
//				{
//					/*concurrency::parallel_sort(begin(patternCombine), end(patternCombine),  
//						[](const std::pair<uint8_t,PListType> &left, const std::pair<uint8_t,PListType> &right) 
//					{
//							return left.second < right.second;
//					});*/
//
//					std::sort(patternCombine.begin(), patternCombine.end(), sort_pred());
//				}
//
//				PListType currentSize = globalLocalPListArray->size();
//				
//				PListType counter = 0;
//				bool newVector = true;
//
//				if(patternCombineSize > 1)
//				{
//					for (PListType k = 0; k < patternCombineSize; k++)
//					{
//						uint8_t val = patternCombine[k].second;
//					
//						if(newVector)
//						{
//							currentSize = globalLocalPListArray->size();
//							globalLocalPListArray->push_back(new vector<PListType>());
//							newVector = false;
//							counter = 0;
//						}
//
//						if(k < patternCombineSize - 1)
//						{
//							if(val == patternCombine[k+1].second)
//							{
//								(*globalLocalPListArray)[currentSize]->push_back(patternCombine[k].first);
//								counter++;
//							}
//							else if(val != patternCombine[k+1].second && counter > 0)
//							{
//								(*globalLocalPListArray)[currentSize]->push_back(patternCombine[k].first);
//								counter = 0;
//								newVector = true;
//							}
//							else if(counter > 0)
//							{
//								(*globalLocalPListArray)[currentSize]->push_back(patternCombine[k].first);
//								counter++;
//							}
//							else
//							{
//								totalTallyRemovedPatterns++;
//							}
//						}
//						else
//						{
//							if(counter > 0)
//							{
//								(*globalLocalPListArray)[currentSize]->push_back(patternCombine[k].first);
//								counter++;
//							}
//							else
//							{
//								totalTallyRemovedPatterns++;
//							}
//						}
//					}
//				}
//
//				if((*globalLocalPListArray)[globalLocalPListArray->size() - 1]->size() == 0)
//				{
//					globalLocalPListArray->pop_back();
//				}
//			}
//			else
//			{
//				totalTallyRemovedPatterns++;
//			}
//			delete (*prevLocalPListArray)[i];
//		}
//	}
//	
//
//	countMutex->lock();
//
//	eradicatedPatterns += totalTallyRemovedPatterns;
//
//	/*if(mostCommonPattern.size() < levelInfo.currLevel)
//	{
//		mostCommonPattern.resize(levelInfo.currLevel);
//		mostCommonPatternCount.resize(levelInfo.currLevel);
//	}
//	for (PListType i = 0; i < globalLocalPListArray->size(); i++)
//	{
//		if((*globalLocalPListArray)[i]->size() > mostCommonPatternCount[levelInfo.currLevel - 1])
//		{
//			mostCommonPatternCount[levelInfo.currLevel - 1] = (*globalLocalPListArray)[i]->size();
//				
//			mostCommonPattern[levelInfo.currLevel - 1] = file->fileString.substr(((*(*globalLocalPListArray)[i])[0] - (levelInfo.currLevel)), levelInfo.currLevel);
//		}
//	}*/
//		
//	if(levelRecordings.size() < levelInfo.currLevel)
//	{
//		levelRecordings.resize(levelInfo.currLevel);
//	}
//	levelRecordings[levelInfo.currLevel - 1] += globalLocalPListArray->size();
//
//	/*if(coverage.size() < levelInfo.currLevel)
//	{
//		coverage.resize(levelInfo.currLevel);
//	}
//	coverage[levelInfo.currLevel - 1] += ((float)(globalLocalPListArray->size()))/((float)file->fileStringSize);*/
//		
//	levelInfo.currLevel++;
//
//	if(levelInfo.currLevel > currentLevelVector[levelInfo.threadIndex])
//	{
//		currentLevelVector[levelInfo.threadIndex] = levelInfo.currLevel;
//	}
//	countMutex->unlock();
//
//	if(globalLocalPListArray->size() == 0 || levelInfo.currLevel - 1 >= maximum)
//	{
//		prevLocalPListArray->clear();
//		prevLocalPListArray->reserve(globalLocalPListArray->size());
//		prevLocalPListArray->swap((*globalLocalPListArray));
//		globalLocalPListArray->reserve(0);
//		continueSearching = false;
//		/*countMutex->lock();
//		availableCores.push_back(levelInfo.coreIndex);
//		countMutex->unlock();*/
//	}
//	else
//	{
//		prevLocalPListArray->clear();
//		prevLocalPListArray->reserve(globalLocalPListArray->size());
//		prevLocalPListArray->swap((*globalLocalPListArray));
//		globalLocalPListArray->reserve(0);
//		continueSearching = true;
//		DispatchNewThreadsRAM(globalLocalPListArray->size(), continueSearching, prevLocalPListArray, levelInfo, isThreadDefuncted);
//	}
//		
//	return continueSearching;
//}


bool Forest::ProcessRAM(vector<vector<PListType>*>* prevLocalPListArray, vector<vector<PListType>*>* globalLocalPListArray, LevelPackage& levelInfo, bool &isThreadDefuncted)
{
	bool continueSearching = true;
	int threadsToDispatch = numThreads - 1;
	PListType totalTallyRemovedPatterns = 0;
	PListType newPatterns = 0;
	if(levelInfo.currLevel == 2)
	{
		PListType earlyApproximation = (file->fileString.size() - eradicatedPatterns)/(prevLocalPListArray->size()*256*(numThreads - 1));
		vector<PListType> newPList[256];
		PListType prevPListSize = prevLocalPListArray->size();
		PListType indexes[256] = {0};
		PListType indexesToPush[256] = {0};
		//Banking off very random patterns
		PListType firstPatternIndex[256] = {0};
		uint8_t prevPattern = 0;
		int listLength = 0;

		for (PListType i = 0; i < prevPListSize; i++)
		{
			vector<PListType>* pList = (*prevLocalPListArray)[i];
			PListType pListLength = (*prevLocalPListArray)[i]->size();
			if((*prevLocalPListArray)[i]->size() > 0)
			{
				prevPattern = (uint8_t)file->fileString[(*pList)[0] - 1];
			}
			
			if(pListLength > 1)
			{		
				for (PListType k = 0; k < pListLength; k++)
				{
					//If pattern is past end of string stream then stop counting this pattern
					PListType index = (*pList)[k];
					if (index < file->fileStringSize)
					{

						uint_fast8_t indexIntoFile = (uint8_t)file->fileString[index];

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
				}
			}
			else
			{
				totalTallyRemovedPatterns++;
			}

			vector<PListType>* pList2 = NULL;
			uint8_t newPattern = 0;
			if(i != prevPListSize - 1 && (*prevLocalPListArray)[i+1]->size() != 0)
			{
				pList2 = (*prevLocalPListArray)[i+1];
				newPattern = (uint8_t)file->fileString[(*pList2)[0] - 1];
			}
			if(i == prevPListSize - 1 || prevPattern != newPattern)
			{
				for (PListType k = 0; k < listLength; k++)
				{
					int insert = indexes[indexesToPush[k]];
					if (insert >= minOccurrence)
					{
						int index = globalLocalPListArray->size();

						globalLocalPListArray->push_back(new vector<PListType>(newPList[indexesToPush[k]]));
						//(*globalLocalPListArray)[index]->insert((*globalLocalPListArray)[index]->end(), newPList[indexesToPush[k]].begin(), newPList[indexesToPush[k]].end());
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
					
				listLength = 0;
			}
			delete (*prevLocalPListArray)[i];
		}

		//newPatterns += globalLocalPListArray->size();

		//PListType earlyApproximation = (file->fileString.size() - eradicatedPatterns)/(prevLocalPListArray->size()*256*(numThreads - 1));
		//vector<PListType> newPList[256];
		//PListType prevPListSize = prevLocalPListArray->size();
		//PListType indexes[256] = {0};
		//PListType indexesToPush[256] = {0};
		////Banking off very random patterns
		//PListType firstPatternIndex[256] = {0};
		//vector<pair<PListType, uint8_t>> patternCombine;
		//int listLength = 0;
		//uint8_t prevPattern = 0;
		//int counterCombine = 0;
		//PListType counter = 0;
		//bool newVector = true;
		//for (PListType i = 0; i < prevPListSize; i++)
		//{
		//	vector<PListType>* pList = (*prevLocalPListArray)[i];
		//	PListType pListLength = (*prevLocalPListArray)[i]->size();

		//	if(i % threadsToDispatch == 0)
		//	{
		//		PListType sizer = 0;
		//		for (PListType z = i; z < threadsToDispatch + i; z++)
		//		{
		//			sizer += (*prevLocalPListArray)[z]->size();
		//		}
		//		if(sizer > patternCombine.size())
		//		{
		//			patternCombine.resize(sizer);
		//		}
		//		counterCombine = 0;
		//	}

		//	if(pListLength > 1)
		//	{
		//		for (PListType k = 0; k < pListLength; k++)
		//		{
		//			PListType index = (*pList)[k];
		//			
		//			if (index < file->fileStringSize)
		//			{
		//				uint8_t indexIntoFile = (uint8_t)file->fileString[index];
		//				patternCombine[counterCombine] =  pair<PListType, uint8_t>(index + 1, indexIntoFile);
		//				counterCombine++;
		//			}
		//			else
		//			{
		//				totalTallyRemovedPatterns++;
		//			}
		//		}

		//		PListType currentSize = globalLocalPListArray->size();
		//	
		//		bool newVector = true;
		//		if(i % threadsToDispatch == threadsToDispatch - 1)
		//		{
		//			if(counterCombine > 2)
		//			{
		//				std::sort(patternCombine.begin(), patternCombine.begin() + counterCombine, sort_pred());
		//			}

		//			if(counterCombine > 1)
		//			{
		//				for (PListType k = 0; k < counterCombine; k++)
		//				{
		//					uint8_t val = patternCombine[k].second;
		//			
		//					if(newVector)
		//					{
		//						currentSize = globalLocalPListArray->size();
		//						globalLocalPListArray->push_back(new vector<PListType>());
		//						newVector = false;
		//						counter = 0;
		//					}

		//					if(k < counterCombine - 1)
		//					{
		//						if(val == patternCombine[k+1].second)
		//						{
		//							(*globalLocalPListArray)[currentSize]->push_back(patternCombine[k].first);
		//							counter++;
		//						}
		//						else if(val != patternCombine[k+1].second && counter > 0)
		//						{
		//							(*globalLocalPListArray)[currentSize]->push_back(patternCombine[k].first);
		//							counter = 0;
		//							newVector = true;
		//						}
		//						else if(counter > 0)
		//						{
		//							(*globalLocalPListArray)[currentSize]->push_back(patternCombine[k].first);
		//							counter++;
		//						}
		//						else
		//						{
		//							totalTallyRemovedPatterns++;
		//						}
		//					}
		//					else
		//					{
		//						if(counter > 0)
		//						{
		//							(*globalLocalPListArray)[currentSize]->push_back(patternCombine[k].first);
		//							counter++;
		//						}
		//						else
		//						{
		//							totalTallyRemovedPatterns++;
		//						}
		//					}
		//				}
		//			}
		//			if(globalLocalPListArray->size() != 0 && (*globalLocalPListArray)[globalLocalPListArray->size() - 1]->size() == 0)
		//			{
		//				globalLocalPListArray->pop_back();
		//			}
		//		}

		//	}
		//	else
		//	{
		//		totalTallyRemovedPatterns++;
		//	}
		//	delete (*prevLocalPListArray)[i];
		//}
	}
	else
	{

		PListType earlyApproximation = (file->fileString.size() - eradicatedPatterns)/(prevLocalPListArray->size()*256*(numThreads - 1));
		vector<PListType> newPList[256];
		PListType prevPListSize = prevLocalPListArray->size();
		PListType indexes[256] = {0};
		PListType indexesToPush[256] = {0};
		//Banking off very random patterns
		register PListType firstPatternIndex[256] = {0};
		PListType preloadedFirstPatternIndexes[20] = {0};
		for (PListType i = 0; i < prevPListSize; i++)
		{
			vector<PListType>* pList = (*prevLocalPListArray)[i];
			PListType pListLength = (*prevLocalPListArray)[i]->size();
			if(pListLength > 1)
			{
				int listLength = 0;


				for (PListType k = 0; k < pListLength; k++)
				{

					PListType index = (*pList)[k];
					
					if (index < file->fileStringSize)
					{
						uint_fast8_t indexIntoFile = (uint8_t)file->fileString[index];
						
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
							//preloadedFirstPatternIndexes[k%20] = index + 1;
							indexes[indexIntoFile]++;
							indexesToPush[listLength++] = indexIntoFile;
						}
					}
					else
					{
						totalTallyRemovedPatterns++;
					}
				}
			
				for (PListType k = 0; k < listLength; k++)
				{
					int insert = indexes[indexesToPush[k]];
					if (insert >= minOccurrence)
					{
						int index = globalLocalPListArray->size();

						globalLocalPListArray->push_back(new vector<PListType>(newPList[indexesToPush[k]]));
						//(*globalLocalPListArray)[index]->insert((*globalLocalPListArray)[index]->end(), newPList[indexesToPush[k]].begin(), newPList[indexesToPush[k]].end());
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
			}
			else
			{
				totalTallyRemovedPatterns++;
			}
			delete (*prevLocalPListArray)[i];
		}
	}
	
	countMutex->lock();

	eradicatedPatterns += totalTallyRemovedPatterns;

	/*if(mostCommonPattern.size() < levelInfo.currLevel)
	{
		mostCommonPattern.resize(levelInfo.currLevel);
		mostCommonPatternCount.resize(levelInfo.currLevel);
	}
	for (PListType i = 0; i < globalLocalPListArray->size(); i++)
	{
		if((*globalLocalPListArray)[i]->size() > mostCommonPatternCount[levelInfo.currLevel - 1])
		{
			mostCommonPatternCount[levelInfo.currLevel - 1] = (*globalLocalPListArray)[i]->size();
				
			mostCommonPattern[levelInfo.currLevel - 1] = file->fileString.substr(((*(*globalLocalPListArray)[i])[0] - (levelInfo.currLevel)), levelInfo.currLevel);
		}
	}*/
		
	if(levelRecordings.size() < levelInfo.currLevel)
	{
		levelRecordings.resize(levelInfo.currLevel);
	}
	levelRecordings[levelInfo.currLevel - 1] += globalLocalPListArray->size();
	//levelRecordings[levelInfo.currLevel - 1] += newPatterns;

	/*if(coverage.size() < levelInfo.currLevel)
	{
		coverage.resize(levelInfo.currLevel);
	}
	coverage[levelInfo.currLevel - 1] += ((float)(globalLocalPListArray->size()))/((float)file->fileStringSize);*/
		
	levelInfo.currLevel++;

	if(levelInfo.currLevel > currentLevelVector[levelInfo.threadIndex])
	{
		currentLevelVector[levelInfo.threadIndex] = levelInfo.currLevel;
	}
	countMutex->unlock();

	if(globalLocalPListArray->size() == 0 || levelInfo.currLevel - 1 >= maximum)
	{
		prevLocalPListArray->clear();
		prevLocalPListArray->reserve(globalLocalPListArray->size());
		prevLocalPListArray->swap((*globalLocalPListArray));
		globalLocalPListArray->reserve(0);
		continueSearching = false;
		/*countMutex->lock();
		availableCores.push_back(levelInfo.coreIndex);
		countMutex->unlock();*/
	}
	else
	{
		prevLocalPListArray->clear();
		prevLocalPListArray->reserve(globalLocalPListArray->size());
		prevLocalPListArray->swap((*globalLocalPListArray));
		globalLocalPListArray->reserve(0);
		continueSearching = true;
		DispatchNewThreadsRAM(globalLocalPListArray->size(), continueSearching, prevLocalPListArray, levelInfo, isThreadDefuncted);
	}
		
	return continueSearching;
}


//bool Forest::ProcessRAM(vector<Duple>& prevLocalPListArray, vector<Duple>& globalLocalPListArray, LevelPackage& levelInfo, bool &isThreadDefuncted)
//{
//	bool continueSearching = true;
//	int threadsToDispatch = numThreads - 1;
//	PListType totalTallyRemovedPatterns = 0;
//	PListType newPatterns = 0;
//	
//	vector<Tuple> patternCombine;
//	PListType counterCombine = prevLocalPListArray.size();
//
//	unordered_map<size_t, PListType> yoslap;
//	vector<size_t> toslap(counterCombine);
//	int levelDec = levelInfo.currLevel - 1;
//	PListType j = 0;
//	PListType fileStringSizer = file->fileStringSize;
//
//	string parser;
//	parser.resize(levelDec);
//	for (PListType i = 0; i < counterCombine; i++)
//	{
//		if (prevLocalPListArray[i].index < fileStringSizer)
//		{
//			parser = file->fileString.substr(prevLocalPListArray[i].index - levelDec, levelInfo.currLevel);
//			prevLocalPListArray[i].index++;
//			
//			prevLocalPListArray[i].pListArrayIndex = hash<string>()(parser);
//
//			//toslap[i] = prevLocalPListArray[i].pListArrayIndex;
//
//			/*yoslap[prevLocalPListArray[i].pListArrayIndex]++;
//			if(yoslap[prevLocalPListArray[i].pListArrayIndex] == 2)
//			{
//				newPatterns++;
//			}*/
//		}
//		else
//		{
//			totalTallyRemovedPatterns++;
//		}
//	}
//	PListType currIndex = 0;
//	for (PListType i = 0; i < counterCombine; i++)
//	{
//		
//		if(yoslap[prevLocalPListArray[i].pListArrayIndex] == 1)
//		{
//			totalTallyRemovedPatterns++;
//		}
//		else
//		{
//			prevLocalPListArray[currIndex].index = prevLocalPListArray[i].index;
//			prevLocalPListArray[currIndex].pListArrayIndex= prevLocalPListArray[i].pListArrayIndex;
//			currIndex++;
//		}
//	}
//	prevLocalPListArray.resize(currIndex);
//
//	/*vector<PListType> patternTypes(counterCombine, 0);
//	patternTypes.clear();
//
//	for (PListType i = 0; i < counterCombine; i++)
//	{
//		patternTypes.push_back(
//	}
//
//	junk_it last = mappy.end();
//
//	for (junk_it it = mappy.begin(); it != last; it++)
//	{
//		if(it->second.size() == 1)
//		{
//			prevLocalPListArray[it->second[0]].index = file->fileStringSize;	
//		}
//		else
//		{
//			newPatterns ++;
//		}
//	}*/
//	
//	countMutex->lock();
//
//	//patternCombine.resize(counterCombine);
//	//PListType j = 0;
//	//for (PListType i = 0; i < counterCombine; i++)
//	//{
//	//	if (prevLocalPListArray[i].index < file->fileStringSize)
//	//	{
//	//		patternCombine[j].patternId = (uint8_t)file->fileString[prevLocalPListArray[i].index];
//	//		patternCombine[j].index = prevLocalPListArray[i].index + 1;
//	//		patternCombine[j].pListArrayIndex = prevLocalPListArray[i].pListArrayIndex;
//	//		//patternCombine[j].order = j;
//	//		j++;
//	//	}
//	//	else
//	//	{
//	//		totalTallyRemovedPatterns++;
//	//	}
//	//}
//
//	//counterCombine = j;
//	//patternCombine.resize(counterCombine);
//	//prevLocalPListArray.resize(counterCombine);
//	////cout << "List size: " << prevLocalPListArray.size() << endl;
//	//if(counterCombine > 2)
//	//{
//	//	std::sort(patternCombine.begin(), patternCombine.begin() + counterCombine, sort_tuple2());
//	//}
//
//	//PListType currIndex = 0;
//	//if(counterCombine > 1)
//	//{
//	//	currIndex = patternCombine[0].pListArrayIndex;
//	//}
//	//PListType matches[256] = {0};
//	//vector<uint8_t> indexesToUse(256, 0);
//	//vector<PListType> locations(256, 0);
//	//vector<PListType> indexers(256, 0);
//	//indexesToUse.clear();
//
//
//	//vector<PListType> pushItOut;
//	//pushItOut.reserve(counterCombine);
//
//	/*PListType followAlong = 0;
//	for (PListType i = 0; i < counterCombine; i++)
//	{
//
//		if(currIndex != patternCombine[i].pListArrayIndex)
//		{
//			for (int j = 0; j < 256; j++)
//			{
//				if(matches[j] == 1)
//				{
//					
//					#if	NEW_PROCESS
//					pushItOut.push_back(patternCombine[locations[j]].order);
//					#else
//					prevLocalPListArray[locations[j]].index = file->fileStringSize;
//					#endif
//					followAlong++;
//				}
//			}
//			newPatterns += indexesToUse.size();
//			indexesToUse.clear();
//			currIndex = patternCombine[i].pListArrayIndex;
//			memset(matches, 0, sizeof(matches));
//		}
//			
//		matches[patternCombine[i].patternId]++;
//		locations[patternCombine[i].patternId] = i;
//			
//
//		if(matches[patternCombine[i].patternId] == 2)
//		{
//			indexesToUse.push_back(patternCombine[i].patternId);
//				
//		}
//		else if(matches[patternCombine[i].patternId] == 1)
//		{
//			indexers[patternCombine[i].patternId] = i;
//		}
//				
//		
//
//		#if	NEW_PROCESS
//		patternCombine[i].pListArrayIndex = indexers[patternCombine[i].patternId];
//		#else
//		prevLocalPListArray[i].index = patternCombine[i].index;
//		prevLocalPListArray[i].pListArrayIndex = indexers[patternCombine[i].patternId];
//		#endif
//	}*/
//
//	//PListType followAlong = 0;
//	//for (PListType i = 0; i < counterCombine; i++)
//	//{
//
//	//	if(currIndex != patternCombine[i].pListArrayIndex)
//	//	{
//	//		for (int j = 0; j < 256; j++)
//	//		{
//	//			if(matches[j] == 1)
//	//			{
//	//				
//	//				#if	NEW_PROCESS
//	//				pushItOut.push_back(patternCombine[locations[j]].order);
//	//				#else
//	//				prevLocalPListArray[locations[j]].index = file->fileStringSize;
//	//				#endif
//	//				followAlong++;
//	//			}
//	//		}
//	//		newPatterns += indexesToUse.size();
//	//		indexesToUse.clear();
//	//		currIndex = patternCombine[i].pListArrayIndex;
//	//		memset(matches, 0, sizeof(matches));
//	//	}
//	//		
//	//	matches[patternCombine[i].patternId]++;
//	//	locations[patternCombine[i].patternId] = i;
//	//		
//
//	//	if(matches[patternCombine[i].patternId] == 2)
//	//	{
//	//		indexesToUse.push_back(patternCombine[i].patternId);
//	//			
//	//	}
//	//	else if(matches[patternCombine[i].patternId] == 1)
//	//	{
//	//		indexers[patternCombine[i].patternId] = i;
//	//	}
//	//			
//	//	
//
//	//	#if	NEW_PROCESS
//	//	patternCombine[i].pListArrayIndex = indexers[patternCombine[i].patternId];
//	//	#else
//	//	prevLocalPListArray[i].index = patternCombine[i].index;
//	//	prevLocalPListArray[i].pListArrayIndex = indexers[patternCombine[i].patternId];
//	//	#endif
//	//}
//
//
//	///*stringstream shitter;
//	//shitter << followAlong << endl;
//	//Logger::WriteLog(shitter.str());
//	//cout << shitter.str();*/
//
//	////Leftovers that are kind of old but still edible
//	//
//	//for (int j = 0; j < 256; j++)
//	//{
//	//	if(matches[j] == 1)
//	//	{
//	//		#if	NEW_PROCESS
//	//		pushItOut.push_back(patternCombine[locations[j]].order);
//	//		#else
//	//		prevLocalPListArray[locations[j]].index = file->fileStringSize;
//	//		#endif
//	//	}
//	//}
//	//newPatterns += indexesToUse.size();
//	
//	
////#if	NEW_PROCESS
////	std::sort(pushItOut.begin(), pushItOut.end());
////	PListType z = 0;
////	PListType insertLocation = 0;
////	
////	/*for (PListType i = 0; i < counterCombine; i++)
////	{
////		if(z < pushItOut.size() && pushItOut[z] == i)
////		{
////			prevLocalPListArray[patternCombine[i].order].index = file->fileStringSize;
////			z++;
////		}
////		else
////		{
////			prevLocalPListArray[patternCombine[i].order].index = patternCombine[i].index;
////			prevLocalPListArray[patternCombine[i].order].pListArrayIndex = patternCombine[i].pListArrayIndex;
////		}
////		
////	}*/
////	for (PListType i = 0; i < counterCombine; i++)
////	{
////		if(z < pushItOut.size() && pushItOut[z] == i)
////		{
////			prevLocalPListArray[patternCombine[i].order].index = file->fileStringSize;
////			z++;
////		}
////		else
////		{
////			prevLocalPListArray[i].index++;
////			prevLocalPListArray[i].pListArrayIndex = patternCombine[i].pListArrayIndex;
////		}
////	}
////#else
////	if(counterCombine > 2)
////	{
////		std::sort(prevLocalPListArray.begin(), prevLocalPListArray.begin() + counterCombine, sort_duple());
////	}
////#endif
//
//	/*shitter.clear();
//	shitter << counterCombine << endl;
//	Logger::WriteLog(shitter.str());
//	cout << shitter.str();*/
//	
//	/*countMutex->lock();*/
//
//	eradicatedPatterns += totalTallyRemovedPatterns;
//
//	/*if(mostCommonPattern.size() < levelInfo.currLevel)
//	{
//		mostCommonPattern.resize(levelInfo.currLevel);
//		mostCommonPatternCount.resize(levelInfo.currLevel);
//	}
//	for (PListType i = 0; i < globalLocalPListArray->size(); i++)
//	{
//		if((*globalLocalPListArray)[i]->size() > mostCommonPatternCount[levelInfo.currLevel - 1])
//		{
//			mostCommonPatternCount[levelInfo.currLevel - 1] = (*globalLocalPListArray)[i]->size();
//				
//			mostCommonPattern[levelInfo.currLevel - 1] = file->fileString.substr(((*(*globalLocalPListArray)[i])[0] - (levelInfo.currLevel)), levelInfo.currLevel);
//		}
//	}*/
//		
//	if(levelRecordings.size() < levelInfo.currLevel)
//	{
//		levelRecordings.resize(levelInfo.currLevel);
//	}
//	//levelRecordings[levelInfo.currLevel - 1] += globalLocalPListArray->size();
//	levelRecordings[levelInfo.currLevel - 1] += newPatterns;
//
//	/*if(coverage.size() < levelInfo.currLevel)
//	{
//		coverage.resize(levelInfo.currLevel);
//	}
//	coverage[levelInfo.currLevel - 1] += ((float)(globalLocalPListArray->size()))/((float)file->fileStringSize);*/
//		
//	levelInfo.currLevel++;
//
//	if(levelInfo.currLevel > currentLevelVector[levelInfo.threadIndex])
//	{
//		currentLevelVector[levelInfo.threadIndex] = levelInfo.currLevel;
//	}
//	countMutex->unlock();
//
//	if(newPatterns == 0 || levelInfo.currLevel - 1 >= maximum)
//	{
//		/*prevLocalPListArray.clear();
//		prevLocalPListArray.reserve(globalLocalPListArray.size());
//		prevLocalPListArray.swap(globalLocalPListArray);
//		globalLocalPListArray.reserve(0);*/
//		continueSearching = false;
//		/*countMutex->lock();
//		availableCores.push_back(levelInfo.coreIndex);
//		countMutex->unlock();*/
//	}
//	else
//	{
//		/*prevLocalPListArray.clear();
//		prevLocalPListArray.reserve(globalLocalPListArray.size());
//		prevLocalPListArray.swap(globalLocalPListArray);
//		globalLocalPListArray.reserve(0);*/
//		continueSearching = true;
//		//DispatchNewThreadsRAM(globalLocalPListArray->size(), continueSearching, prevLocalPListArray, levelInfo, isThreadDefuncted);
//	}
//		
//	return continueSearching;
//}



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

//void Forest::ThreadedLevelTreeSearchRecursionList(vector<vector<PListType>*>* patterns, vector<PListType> patternIndexList, vector<string> fileList, LevelPackage levelInfo)
//{
//	PListType numPatternsToSearch = patternIndexList.size();
//	bool isThreadDefuncted = false;
//	//cout << "Threads dispatched: " << threadsDispatched << " Threads deported: " << threadsDefuncted << " Threads running: " << threadsDispatched - threadsDefuncted << endl;
//	
//	int tempCurrentLevel = levelInfo.currLevel;
//	int threadsToDispatch = numThreads - 1;
//
//	if(threadsDispatched - threadsDefuncted > threadsToDispatch)
//	{
//		cout << "WENT OVER THREADS ALLOCATION SIZE!" << endl;
//	}
//	
//	vector<Duple> prevLocalPListArray;
//	vector<Duple> globalLocalPListArray;
//
//	if(levelInfo.useRAM)
//	{
//		PListType combineCount = 0;
//		for(PListType i = 0; i < numPatternsToSearch; i++)
//		{
//			if((*patterns)[patternIndexList[i]] != NULL)
//			{
//				combineCount += (*patterns)[patternIndexList[i]]->size();
//			}
//		}
//		prevLocalPListArray.resize(combineCount);
//		combineCount = 0;
//		int threadIndex = 0;
//		for(PListType i = 0; i < numPatternsToSearch; i++)
//		{
//			if(i % threadsToDispatch == 0)
//			{
//				threadIndex++;
//			}
//			if((*patterns)[patternIndexList[i]] != NULL)
//			{
//				for(PListType j = 0; j < (*patterns)[patternIndexList[i]]->size(); j++)
//				{
//					prevLocalPListArray[combineCount].index = (*(*patterns)[patternIndexList[i]])[j];
//					prevLocalPListArray[combineCount++].pListArrayIndex = threadIndex;
//				}
//			}
//		}
//	}
//
//	std::sort(prevLocalPListArray.begin(), prevLocalPListArray.end(), sort_duple());
//	
//	bool continueSearching = true;
//
//	while(continueSearching)
//	{
//		bool useRAMBRO = true;
//		/*if(levelInfo.currLevel != 2)
//		{
//			useRAMBRO = !NextLevelTreeSearchRecursion(prevLocalPListArray, globalLocalPListArray, fileList, levelInfo);
//		}
//		else
//		{*/
//			useRAMBRO = levelInfo.useRAM;
//		//}
//
//		if(useRAMBRO)
//		{
//			continueSearching = ProcessRAM(prevLocalPListArray, globalLocalPListArray, levelInfo, isThreadDefuncted);
//		}
//		else
//		{
//			continueSearching = ProcessHD(levelInfo, fileList, isThreadDefuncted);
//		}
//	}
//
//	/*if(prevLocalPListArray != NULL)
//	{
//		delete prevLocalPListArray;
//	}
//
//	if(globalLocalPListArray != NULL)
//	{
//		delete globalLocalPListArray;
//	}*/
//	
//	countMutex->lock();
//	if(currentLevelVector[levelInfo.threadIndex] > globalLevel)
//	{
//		globalLevel = currentLevelVector[levelInfo.threadIndex];
//	}
//	if(!isThreadDefuncted)
//	{
//		threadsDefuncted++;
//	}
//	countMutex->unlock();
//
//	return;
//}

void Forest::PlantTreeSeedThreadHD(PListType positionInFile, PListType startPatternIndex, PListType numPatternsToSearch, unsigned int threadNum)
{
	LevelPackage levelInfo;
	levelInfo.currLevel = globalLevel;
	levelInfo.coreIndex = threadNum;
	levelInfo.threadIndex = threadNum;

	PListType memDivisorInMB = (PListType)(memoryPerThread/3.0f);
	//used primarily for just storage containment
	TreeHD leaf;
	for (PListType i = startPatternIndex; i < numPatternsToSearch + startPatternIndex; i++)
	{
#ifdef INTEGERS
		
		stringstream finalValue;
		//If pattern is past end of string stream then stop counting this pattern
		if (i < file->fileStringSize)
		{
			while(i < file->fileStringSize)
			{
				unsigned char value = file->fileString[i];
				//if values are between 0 through 9 and include 45 for the negative sign
				if(value >= 48 && value <= 57 || value == 45)
				{
					finalValue << value;
				}
			

				if(value == '\r' || value == 13 || value == '\n' || value == ' ' || value == '/t')
				{
					while((value < 48 || value > 57) && value != 45 && i < file->fileStringSize)
					{
						value = file->fileString[i];
						i++;
					}
					if(i < file->fileStringSize)
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

		if(patternToSearchFor.size() == 0 || file->fileString.substr(i, 1)[0] == patternToSearchFor[0])
		{
			leaf.addLeaf(i+positionInFile+1, file->fileString.substr(i, 1));
		}
		if(overMemoryCount)
		{
			stringstream stringBuilder;
			fileIDMutex->lock();
			fileID++;
			stringBuilder << fileID;
			fileIDMutex->unlock();
			newFileNameList[threadNum].push_back(CreateChunkFile(stringBuilder.str(), leaf, levelInfo/*, threadNum, globalLevel*/));
		}
#endif
	}

	
	if(leaf.leaves.size() > 0)
	{
		stringstream stringBuilder;
		fileIDMutex->lock();
		fileID++;
		stringBuilder << fileID;
		fileIDMutex->unlock();
		newFileNameList[threadNum].push_back(CreateChunkFile(stringBuilder.str(), leaf, levelInfo/*threadNum, globalLevel*/));
	}

	return;
}

void Forest::PlantTreeSeedThreadRAM(PListType positionInFile, PListType startPatternIndex, PListType numPatternsToSearch, PListType threadIndex)
{
	int threadsToDispatch = numThreads - 1;
	PListType earlyApproximation = file->fileString.size()/(256*(numThreads - 1));
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
		if (i < file->fileStringSize)
		{
			while(i < file->fileStringSize)
			{
				unsigned char value = file->fileString[i];
				//if values are between 0 through 9 and include 45 for the negative sign
				if(value >= 48 && value <= 57 || value == 45)
				{
					finalValue << value;
				}
			

				if(value == '\r' || value == 13 || value == '\n' || value == ' ' || value == '/t')
				{
					while((value < 48 || value > 57) && value != 45 && i < file->fileStringSize)
					{
						value = file->fileString[i];
						i++;
					}
					if(i < file->fileStringSize)
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

		/*if(i%20 == 0)
		{
			_mm_prefetch((const char *)&file->fileString[i], _MM_HINT_T0);
		}*/

		int temp = i + 1;
		uint8_t tempIndex = (uint8_t)file->fileString[i];
		if(patternToSearchFor.size() == 0 || file->fileString[i] == patternToSearchFor[0])
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

//TreeRAM* Forest::PlantTreeSeedThreadRAM(PListType positionInFile, PListType startPatternIndex, PListType numPatternsToSearch)
//{
//	int currentThread = startPatternIndex/numPatternsToSearch;
//	//used primarily for just storage containment
//	PListType earlyApproximation = file->fileString.size()/(256*(numThreads - 1));
//	vector<vector<PListType>*> newPList(256);
//	for (int i = 0; i < 256; i++)
//	{
//		newPList[i] = new vector<PListType>();
//		//newPList[i]->reserve(earlyApproximation);
//	}
//
//	for (PListType i = startPatternIndex; i < numPatternsToSearch + startPatternIndex; i++)
//	{
//#ifdef INTEGERS
//		
//		stringstream finalValue;
//		//If pattern is past end of string stream then stop counting this pattern
//		if (i < file->fileStringSize)
//		{
//			while(i < file->fileStringSize)
//			{
//				unsigned char value = file->fileString[i];
//				//if values are between 0 through 9 and include 45 for the negative sign
//				if(value >= 48 && value <= 57 || value == 45)
//				{
//					finalValue << value;
//				}
//			
//
//				if(value == '\r' || value == 13 || value == '\n' || value == ' ' || value == '/t')
//				{
//					while((value < 48 || value > 57) && value != 45 && i < file->fileStringSize)
//					{
//						value = file->fileString[i];
//						i++;
//					}
//					if(i < file->fileStringSize)
//					{
//						i-=2;
//					}
//					break;
//				}
//				else
//				{
//					i++;
//				}
//			}
//			if(finalValue.str() != "")
//			{
//				string patternValue = finalValue.str();
//				unsigned long long ull = stoull(patternValue, &sz);
//				//cout << "Pattern found: " << ull << endl;
//				leaf->addLeaf(ull, i + 1, patternValue);
//			}
//		}
//#endif
//#ifdef BYTES
//		
//		if(patternToSearchFor.size() == 0 || file->fileString[i] == patternToSearchFor[0])
//		{
//			newPList[(uint8_t)file->fileString[i]]->push_back(i + 1);	
//		}
//
//#endif
//
//	}
//	vector<int> listToDos;
//	for(int i = 0; i < 256; i++)
//	{
//		if(gatedMutexes[i]->try_lock())
//		{
//			(*prevPListArray)[i]->insert((*prevPListArray)[i]->end(), newPList[i]->begin(), newPList[i]->end());
//			//(*localPrevPListArrayTriplet)[i]->push_back(newPList[i]);
//			gatedMutexes[i]->unlock();
//		}
//		else
//		{
//			listToDos.push_back(i);
//		}
//	}
//	while(listToDos.size() > 0)
//	{
//		vector<int> nextListToDos;
//		for(int i = 0; i < listToDos.size(); i++)
//		{
//			if(gatedMutexes[listToDos[i]]->try_lock())
//			{
//				(*prevPListArray)[listToDos[i]]->insert((*prevPListArray)[listToDos[i]]->end(), newPList[listToDos[i]]->begin(), newPList[listToDos[i]]->end());
//				//(*localPrevPListArrayTriplet)[i]->push_back(newPList[listToDos[i]]);
//				gatedMutexes[listToDos[i]]->unlock();
//			}
//			else
//			{
//				nextListToDos.push_back(listToDos[i]);
//			}
//		}
//		listToDos.clear();
//		listToDos.resize(nextListToDos.size());
//		for(int k = 0; k < nextListToDos.size(); k++)
//		{
//			listToDos[k] = nextListToDos[k];
//		}
//	}
//
//
//	return NULL;
//}
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
		if(archiveCollective->totalWritten >= 262144 && overMemoryCount) 
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
	
		if( remove( fileNameToBeRemoved.c_str() ) != 0)
		{
			/*stringstream builder;
			builder << "Chunks Failed to delete " << fileNameToBeRemoved << ": " << strerror(errno) << '\n';
			Logger::WriteLog(builder.str());*/
		}
		else
		{
			/*stringstream builder;
			builder << "Chunk succesfully deleted " << fileNameToBeRemoved << '\n';
			Logger::WriteLog(builder.str());*/
		}

		string fileNameToBeRemovedPatterns = folderLocation;
		fileNameToBeRemovedPatterns.append(fileNames[i].c_str());
		fileNameToBeRemovedPatterns.append("Patterns.txt");
	
		if( remove( fileNameToBeRemovedPatterns.c_str() ) != 0)
		{
			/*stringstream builder;
			builder << "Chunks Failed to delete " << fileNameToBeRemovedPatterns << ": " << strerror(errno) << '\n';
			Logger::WriteLog(builder.str());*/
			
		}
		else
		{
			/*stringstream builder;
			builder << "Chunk succesfully deleted " << fileNameToBeRemovedPatterns << '\n';
			Logger::WriteLog(builder.str());*/
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
	
		if( remove( fileNameToBeRemoved.c_str() ) != 0)
		{
			/*stringstream builder;
			builder << "Archives Failed to delete '" << fileNameToBeRemoved << "': " << strerror(errno) << '\n';
			Logger::WriteLog(builder.str());*/
		}
		else
		{
			/*stringstream builder;
			builder << "Archives succesfully deleted " << fileNameToBeRemoved << '\n';
			Logger::WriteLog(builder.str());*/
		}
	}
}

void Forest::DeleteArchive(string fileNames, string folderLocation)
{
	string fileNameToBeRemoved = folderLocation;
	fileNameToBeRemoved.append(fileNames.c_str());
	fileNameToBeRemoved.append(".txt");
	
	if( remove( fileNameToBeRemoved.c_str() ) != 0)
	{
		/*stringstream builder;
		builder << "Archive Failed to delete '" << fileNameToBeRemoved << "': " << strerror(errno) << '\n';
		Logger::WriteLog(builder.str());*/
	}
	else
	{
		/*stringstream builder;
		builder << "Archive succesfully deleted " << fileNameToBeRemoved << '\n';
		Logger::WriteLog(builder.str());*/
	}
}
void Forest::DeleteChunk(string fileChunkName, string folderLocation)
{
	
	string fileNameToBeRemoved = folderLocation;
	fileNameToBeRemoved.append(fileChunkName.c_str());
	fileNameToBeRemoved.append(".txt");
	
	if( remove( fileNameToBeRemoved.c_str() ) != 0)
	{
		/*stringstream builder;
		builder << "Chunk Failed to delete " << fileNameToBeRemoved << ": " << strerror(errno) << '\n';
		Logger::WriteLog(builder.str());*/
	}
	else
	{
		/*stringstream builder;
		builder << "Chunk succesfully deleted " << fileNameToBeRemoved << '\n';
		Logger::WriteLog(builder.str());*/
	}

	string fileNameToBeRemovedPatterns = folderLocation;
	fileNameToBeRemovedPatterns.append(fileChunkName.c_str());
	fileNameToBeRemovedPatterns.append("Patterns.txt");
	
	if( remove( fileNameToBeRemovedPatterns.c_str() ) != 0)
	{
		/*stringstream builder;
		builder << "Chunk Failed to delete '" << fileNameToBeRemovedPatterns << "': " << strerror(errno) << '\n';
		Logger::WriteLog(builder.str());*/
	}
	else
	{
		/*stringstream builder;
		builder << "Chunk succesfully deleted " << fileNameToBeRemovedPatterns << '\n';
		Logger::WriteLog(builder.str());*/
	}
	
}