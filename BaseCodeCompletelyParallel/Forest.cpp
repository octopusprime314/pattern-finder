#include "Forest.h"
#include "MemoryUtils.h"
#if defined(_WIN64) || defined(_WIN32)
	#include <direct.h>
#elif defined(__linux__)
	#include "sys/stat.h"
#endif

Forest::Forest(int argc, char **argv)
{
	//system("rm -r ../Log/*");
	//mkdir("../Log/BackupLog");

	threadsDispatched = 0;
	threadsDefuncted = 0;

	displayEachLevelSearch = false;
	findBestThreadNumber = false;
	threadPool = new vector<future<void>>();
	threadPlantSeedPoolHD = new vector<future<TreeHD*>>();
	threadPlantSeedPoolRAM = new vector<future<TreeRAM*>>();
	globalLevel = 1;
	levelCountPatterns = 0;
	usingMemoryBandwidth = false;
	memoryBandwidthMB = 0;
	countMutex = new mutex();
	levelToOutput = 0;
	history = false;
	sizeOfPreviousLevelMB = 0;
	eradicatedPatterns = 0;

	usingPureRAM = false;
	usingPureHD = false;
	startingLevel = 1;

	MemoryUsageAtInception = MemoryUtils::GetProgramMemoryConsumption();
	
	prevPListArray = new vector<vector<PListType>*>();
	globalPListArray = new vector<vector<PListType>*>();

	//Initialize all possible values for the first list to NULL
	for(int i = 0; i < 256; i++)
	{
		gatedMutexes.push_back(new mutex());
	}

	CommandLineParser(argc, argv);

	//Assume start with RAM
	usedRAM = true;

	//main thread is a hardware thread so dispatch threads requested minus 1
	int threadsToDispatch = 0;
	if(findBestThreadNumber)
	{
		numThreads = 2;
	}
	threadsToDispatch = numThreads - 1;
	
	
	//If memory bandwidth not an input
	if(!usingMemoryBandwidth)
	{
		//Leave 1 GB to spare for operating system in case our calculations suck
		memoryBandwidthMB = MemoryUtils::GetAvailableRAMMB() - 1000;
	}

	

	for(int threadIteration = 0; threadIteration < testIterations; threadIteration = numThreads)
	{
		//Initialize all possible values for the first list to NULL
		for(int i = 0; i < 256; i++)
		{
			prevPListArray->push_back(NULL);
		}
		memoryPerThread = memoryBandwidthMB/threadsToDispatch;
		cout << "Memory that can be used per thread: " << memoryPerThread << " MB." << endl;

		StopWatch time;

		for(int z = 0; z < threadsToDispatch; z++)
		{
			currentLevelVector.push_back(0);
			activeThreads.push_back(false);
		}

		PListType overallFilePosition = 0;

		//must divide by the size each index is stored as
		memoryPerCycle = (memoryBandwidthMB*1000000)/sizeof(PListType);
		PListType fileIterations = file->fileStringSize/memoryPerCycle;
		if(file->fileStringSize%memoryPerCycle != 0)
		{
			fileIterations++;
		}

		//If using pure ram we don't care about memory constraints
		if(usingPureRAM)
		{
			fileIterations = 1;
		}

		//make this value 1 so calculations work correctly then reset
		levelCountPatterns = 1;
		bool prediction = PredictHardDiskOrRAMProcessing();
		levelCountPatterns = 0;

		vector<string> backupFilenames;

		file->copyBuffer->clear();
		file->copyBuffer->seekg(0, ios::beg);

		for(int z = 0; z < fileIterations; z++)
		{
			cout << "Number of threads processing file is " << threadsToDispatch << endl;
			PListType position = 0;
			PListType patternCount = 0;
			if(file->fileStringSize <= memoryPerCycle*z + memoryPerCycle)
			{
				patternCount = file->fileStringSize - memoryPerCycle*z;
			}
			else
			{
				patternCount = memoryPerCycle;
			}

			//load in the entire file
			if(usingPureRAM)
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

			MemoryUsedPriorToThread = MemoryUtils::GetProgramMemoryConsumption();

			for(int i = 0; i < threadsToDispatch; i++)
			{
				if(!(i < threadsToDispatch - 1))
				{
					span = span + lastCycle;
				}	

				if(prediction)
				{
					threadPlantSeedPoolHD->push_back(std::async(std::launch::async, &Forest::PlantTreeSeedThreadHD, this, overallFilePosition, position, span, i));
				}
				else
				{
					//threadPlantSeedPoolRAM->push_back(std::async(std::launch::async, &Forest::PlantTreeSeedThreadDynamicRAM, this, overallFilePosition, position, span, startingLevel));
					threadPlantSeedPoolRAM->push_back(std::async(std::launch::async, &Forest::PlantTreeSeedThreadRAM, this, overallFilePosition, position, span));
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
				int threadsFinished = 0;
				unsigned int threadsToDispatch = numThreads - 1;
				while (threadsFinished != threadsToDispatch)
				{
					for (int k = 0; k < threadsToDispatch; k++)
					{
						if (threadPlantSeedPoolRAM != NULL)
						{
							TreeRAM* temp = (*threadPlantSeedPoolRAM)[k].get();
							threadsFinished++;
						}
					}
				}

				typedef std::map<string, vector<PListType>*>::iterator it_type2;
				PListType index = 0;
				for(it_type2 iterator = globalMap.begin(); iterator != globalMap.end(); iterator++)
				{
					prevPListArray->push_back(NULL);
					(*prevPListArray)[index] = iterator->second;
				}

				levelCountPatterns = prevPListArray->size();
			}
			overallFilePosition += position;

			if(prediction)
			{
				threadPlantSeedPoolHD->erase(threadPlantSeedPoolHD->begin(), threadPlantSeedPoolHD->end());
				(*threadPlantSeedPoolHD).clear();
			}
			else
			{
				threadPlantSeedPoolRAM->erase(threadPlantSeedPoolRAM->begin(), threadPlantSeedPoolRAM->end());
				(*threadPlantSeedPoolRAM).clear();
			}
			
		}

		//If not conserving memory dont clear out for next level processing
		if(!usingPureRAM)
		{
			//Close file handle once and for all
			file->copyBuffer->clear();
			file->fileString.clear();
			file->fileString.resize(0);
		}

		typedef std::map<PatternType, PListType>::iterator it_map_type;

		map<PatternType, PListType> finalMetaDataMap;
		vector<PListArchive*> archiveCollection;
		//Divide between file load and previous level pLists and leave some for new lists haha 
		unsigned long long memDivisor = (memoryPerThread*1000000)/3.0f;

		int currentFile = 0;
		bool memoryOverflow = false;

		if(prediction)
		{
			if(levelToOutput == 0 || (levelToOutput != 0 && globalLevel >= levelToOutput))
			{
				//Process for all threads
				for(int piss = 0; piss < threadsToDispatch; piss++)
				{
					for(int z = 0; z < newFileNameList[piss].size(); z++)
					{
						backupFilenames.push_back(newFileNameList[piss][z]);
					}
				}
				ProcessChunksAndGenerate(backupFilenames, memDivisor, numThreads - 1, true);
			}
		
			if(!history)
			{
				DeleteChunks(backupFilenames, ARCHIVE_FOLDER);
			}
		}
		else
		{
			//nothing for now
		}

		DisplayPatternsFound();
		
		bool end = false;
		for (int i = 2; end == false; i++)
		{
			globalLevel = i;
		
			if (i <= maximum && NextLevelTreeSearch(i))
			{

			}
			else
			{
				for(int j = 2; j < globalLevel; j++)
				{
					stringstream buff;
					buff << "Level: " << j << " count is " << levelRecordings[j] << endl;
					Logger::WriteLog(buff.str());
					cout << buff.str();
				}
				end = true;
			}
		}
		currentLevelVector.clear();

		for (int i = 0; i < prevPListArray->size(); i++)
		{
			delete (*prevPListArray)[i];
		}
		prevPListArray->clear();
		
		for (int i = 0; i < globalPListArray->size(); i++)
		{
			delete (*globalPListArray)[i];
		}
		globalPListArray->clear();

		time.Stop();
		time.Display();
		cout << threadsToDispatch << " threads were used to process file" << endl;

		numThreads = (numThreads * 2) - 1;
		threadsToDispatch = numThreads - 1;
		
		//reset global level in case we are testing
		globalLevel = 1;
	}

	//Close file handle once and for all
	file->copyBuffer->clear();
	file->copyBuffer->close();
	delete file->copyBuffer;

	file->fileString.clear();
	file->fileString.resize(0);
	file->fileString = "";
	delete file;
	
	delete threadPool;
	delete threadPlantSeedPoolHD;
	delete threadPlantSeedPoolRAM;

	delete countMutex;
	delete prevPListArray;
	delete globalPListArray;

	initTime.Stop();
	initTime.Display();
}

Forest::~Forest()
{
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

void Forest::FirstLevelRAMProcessing()
{
	int threadsFinished = 0;
	unsigned int threadsToDispatch = numThreads - 1;
	map<char, TreeRAM*>* globalMap = new map<char, TreeRAM*>();
	while (threadsFinished != threadsToDispatch)
	{
		for (int k = 0; k < threadsToDispatch; k++)
		{
			if (threadPlantSeedPoolRAM != NULL)
			{
				TreeRAM* temp = (*threadPlantSeedPoolRAM)[k].get();
				map<char, TreeRAM*> map = temp->GetMap();

				typedef std::map<char, TreeRAM*>::iterator it_type;
				for (it_type iterator = map.begin(); iterator != map.end(); iterator++)
				{
					if (globalMap->find(iterator->first) == globalMap->end())
					{
						vector<PListType>* newPList = iterator->second->GetPList();
						(*globalMap)[iterator->first] = iterator->second;
					}
					else
					{
						vector<PListType>* newPList = iterator->second->GetPList();
						vector<PListType>* currGlobalPList = (*globalMap)[iterator->first]->GetPList();

						currGlobalPList->insert(currGlobalPList->end(), newPList->begin(), newPList->end());

						delete iterator->second->GetPList();
						delete iterator->second;

					}
				}

				delete temp->GetPList();
				delete temp;

				threadsFinished++;

			}
		}
	}
	prevPListArray->clear();

	typedef std::map<char, TreeRAM*>::iterator it_type;
	//Remove map but extract pLists...hungry why wait grab a snickers.
	for(it_type iterator = globalMap->begin(); iterator != globalMap->end(); iterator++)
	{
		vector<PListType>* pList = iterator->second->GetPList();
		if(pList->size() <= 1)
		{
			delete pList;
		}
		else
		{
			stringstream builder;
			builder << "Pattern " << (*iterator).first << " occurs " << pList->size() << endl;
			Logger::WriteLog(builder.str());
			prevPListArray->push_back(pList);
		}
	}
	levelCountPatterns = prevPListArray->size();

	delete globalMap;
}

void Forest::FirstLevelHardDiskProcessing(vector<string>& backupFilenames, unsigned int z)
{
	map<PatternType, TreeHD*>* globalMap = new map<PatternType, TreeHD*>();;
	unsigned int threadsToDispatch = numThreads - 1;
	int threadsFinished = 0;
	while(threadsFinished != threadsToDispatch)
	{
		for(int k = 0; k < threadsToDispatch; k++)
		{
			if(threadPlantSeedPoolHD != NULL)
			{
				TreeHD* temp = (*threadPlantSeedPoolHD)[k].get();
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

	//All files need to be placed in data folder relative to your executable
	string tempFileName = DATA_FOLDER;

	for (int i = 0; i < argc; i++)
	{
		string arg(argv[i]);
		if (arg.compare("/min") == 0 || arg.compare("/Min") == 0 || arg.compare("/MIN") == 0)
		{
			// We know the next argument *should* be the minimum pattern to display
			minimum = atoi(argv[i + 1]);
			maxEnter = true;
		}
		else if (arg.compare("/max") == 0 || arg.compare("/Max") == 0 || arg.compare("/MAX") == 0)
		{
			// We know the next argument *should* be the maximum pattern to display
			maximum = atoi(argv[i + 1]);
			levelRecordings.resize(maximum);
			for(int j = 0; j < levelRecordings.size(); j++)
			{
				levelRecordings[j] = 0;
			}
			minEnter = true;
		}
		else if (arg.compare("/f") == 0 || arg.compare("/F") == 0)
		{
			// We know the next argument *should* be the filename
			tempFileName.append(argv[i + 1]);
			file = new FileReader(tempFileName);
			fileEnter = true;
		}
		else if (arg.compare("/d") == 0 || arg.compare("/D") == 0)
		{
			displayEachLevelSearch = true;
		}
		else if (arg.compare("/c") == 0 || arg.compare("/D") == 0)
		{
			findBestThreadNumber = true;
		}
		else if (arg.compare("/THREADS") == 0 || arg.compare("/threads") == 0 || arg.compare("/Threads") == 0)
		{
			// We know the next argument *should* be the maximum pattern to display
			numThreads = atoi(argv[i + 1]);
			threadsEnter = true;
		}
		else if(arg.compare("/mem") == 0 || arg.compare("/Mem") == 0)
		{
			memoryBandwidthMB = atoi(argv[i + 1]);
			usingMemoryBandwidth = true;
		}
		else if(arg.compare("/lev") == 0 || arg.compare("/Lev") == 0)
		{
			levelToOutput = atoi(argv[i + 1]);
		}
		else if(arg.compare("/his") == 0 || arg.compare("/His") == 0)
		{
			history = atoi(argv[i + 1]);
		}
		else if(arg.compare("/RAM") == 0 || arg.compare("/ram") == 0)
		{
			usingPureRAM = true;
		}
		else if(arg.compare("/HD") == 0 || arg.compare("/hd") == 0)
		{
			usingPureHD = true;
		}
		else if(arg.compare("/Start") == 0 || arg.compare("/start") == 0)
		{
			startingLevel = atoi(argv[i + 1]);
		}
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

bool Forest::PredictHardDiskOrRAMProcessing()
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
	PListType potentialPatterns = 0;
	
	//main thread is a hardware thread so dispatch threads requested minus 1
	PListType threadsToDispatch = numThreads - 1;
	//predictedMemoryForLevelProcessing has to include memory loading in from previous level and memory for the next level
	//First calculate the size of the file because that is the maximum number of pLists we can store minus the level
	//Second calculate the size of Tree objects will be allocated based on the number of POTENTIAL PATTERNS...
	//POTENTIAL PATTERNS equals the previous list times 256 possible byte values but this value can't exceed the file size minus the current level
	if(usedRAM)
	{
		potentialPatterns = prevPListArray->size()*256;
	}
	else
	{
		potentialPatterns = levelCountPatterns*256;
	}

	cout << "Eradicated file indexes: " << eradicatedPatterns << endl;
	if(potentialPatterns > file->fileStringSize - (globalLevel - 1))
	{
		//Factor in eradicated patterns because those places no longer need to be checked in the file
		potentialPatterns = (file->fileStringSize - eradicatedPatterns) - (globalLevel - 1);
		cout << "Potential patterns has exceed file size so we downgrade to " << potentialPatterns << endl;
	}

	cout << "Potential patterns for level " << globalLevel << " is " << potentialPatterns << endl;

	PListType sizeOfTreeMap = 0;
	if(usedRAM)
	{
		sizeOfTreeMap = sizeof(char)*potentialPatterns + sizeof(TreeRAM)*potentialPatterns;
	}
	else
	{
		sizeOfTreeMap = sizeof(char)*globalLevel*potentialPatterns + sizeof(TreeHD)*potentialPatterns;
	}

	PListType vectorSize = levelCountPatterns*sizeof(vector<PListType>*);
	PListType pListIndexesLeft = (file->fileStringSize - eradicatedPatterns)*sizeof(PListType);
	PListType predictedMemoryForNextLevelMB = (vectorSize + pListIndexesLeft + sizeOfTreeMap)/1000000.0f;

	cout << "Predicted size for level " << globalLevel << " is " << predictedMemoryForNextLevelMB << " MB" << endl;
	
	PListType predictedMemoryForLevelProcessingMB = 0;
	PListType previousLevelMemoryMB = 0;
	PListType predictedMemoryForLevelProcessing = 0;
		
	if(usedRAM)
	{
		previousLevelMemoryMB = MemoryUtils::GetProgramMemoryConsumption() - MemoryUsageAtInception;
	}
	else
	{
		//If patterns calculated previously then use that data
		if(levelCountPatterns > 0 && globalLevel > 1)
		{
			PListType sizeOfTreeMap = sizeof(char)*levelCountPatterns + sizeof(TreeHD)*levelCountPatterns;
			PListType prevTreeSizeMB = (vectorSize + pListIndexesLeft + sizeOfTreeMap)/1000000.0f;
			cout << "Previous Tree mapping for level " << globalLevel - 1 << " is " << prevTreeSizeMB << " MB" << endl;
			previousLevelMemoryMB = prevTreeSizeMB;
		}
		//If not rely on predictions which can be crude because of possible pattern overlap
		else
		{
			previousLevelMemoryMB = sizeOfPreviousLevelMB;
		}
	}

	cout << "Size used for previous level " << globalLevel - 1 << " is " << previousLevelMemoryMB << " MB" << endl;
	
	predictedMemoryForLevelProcessing = previousLevelMemoryMB + predictedMemoryForNextLevelMB;
	if(predictedMemoryForLevelProcessing > memoryBandwidthMB)
	{
		cout << "Using HARD DISK! Total size for level " << globalLevel << " processing is " << predictedMemoryForLevelProcessing << " MB" << endl;
		return true;
	}
	else
	{
		cout << "Using RAM! Total size for level " << globalLevel << " processing is " << predictedMemoryForLevelProcessing << " MB" << endl;
		return false;
	}
}

bool Forest::NextLevelTreeSearch(PListType level)
{
	
	bool localUsingRam = true;
	PListType threadsToDispatch = numThreads - 1;
	for(int j = 0; j < levelRecordings.size(); j++)
	{
		levelRecordings[j] = 0;
	}
	
	bool prediction = PredictHardDiskOrRAMProcessing();
	if(prediction)
	{
		if(usedRAM)
		{
			//chunk files
			vector<PListArchive*> threadFiles;
			stringstream threadFilesNames;
			unsigned int threadNumber = 0;
			for(int a = 0; a < threadsToDispatch; a++)
			{
				prevFileNameList[a].clear();
			}
			for(int a = 0; a < threadsToDispatch; a++)
			{
				threadFilesNames.str("");
				threadFilesNames << "PListArchive" << a;

				string fileNameage(threadFilesNames.str());
				fileNameage.insert(0, ARCHIVE_FOLDER);
				fileNameage.append(".txt");
				ofstream outputFile(fileNameage);
				outputFile.close();

				threadFiles.push_back(new PListArchive(threadFilesNames.str()));
				prevFileNameList[a].push_back(threadFilesNames.str());
			}
			for(PListType prevIndex = 0; prevIndex < prevPListArray->size(); prevIndex++)
			{
				threadFiles[threadNumber]->WriteArchiveMapMMAP((*prevPListArray)[prevIndex]);
				delete (*prevPListArray)[prevIndex];
				
				//Increment chunk
				threadNumber++;
				if(threadNumber >= threadsToDispatch)
				{
					threadNumber = 0;
				}
			}
			//Clear out the array also after deletion
			prevPListArray->clear();
			
			for(int a = 0; a < threadsToDispatch; a++)
			{
				threadFiles[a]->WriteArchiveMapMMAP(NULL, "", true);
				threadFiles[a]->CloseArchiveMMAP();
				delete threadFiles[a];
			}
		}
		usedRAM = false;
		localUsingRam = false;
	}
	else
	{
		if(!usedRAM)
		{
			for(PListType i = 0; i < prevFileNameList.size(); i++)
			{
				for(PListType prevChunkCount = 0; prevChunkCount < prevFileNameList[i].size(); prevChunkCount++)
				{
					PListArchive archive(prevFileNameList[i][prevChunkCount]);
					while(!archive.IsEndOfFile())
					{
						//Just use 100 GB to say we want the whole file for now
						vector<vector<PListType>*>* packedPListArray = archive.GetPListArchiveMMAP(10000000.0f);
						prevPListArray->insert(prevPListArray->end(), packedPListArray->begin(), packedPListArray->end());
						
						packedPListArray->erase(packedPListArray->begin(), packedPListArray->end());
						delete packedPListArray;
					}
					archive.CloseArchiveMMAP();
				}
			}

			for(PListType i = 0; i < threadsToDispatch; i++)
			{
				if(!history)
				{
					DeleteChunks(prevFileNameList[i], ARCHIVE_FOLDER);
				}
			}
		}
		usedRAM = true;
		localUsingRam = true;
	}


	bool continueSearching = false;
	levelCountPatterns = 0;
	sizeOfPreviousLevelMB = 0;

	MemoryUsedPriorToThread = MemoryUtils::GetProgramMemoryConsumption();
	//cout << "Memory used prior to thread processing " << MemoryUsedPriorToThread << " MB" << endl;
	if(!localUsingRam)
	{
		for (PListType i = 0; i < threadsToDispatch; i++)
		{
			threadPool->push_back(std::async(std::launch::async, &Forest::ThreadedLevelTreeSearch, this, i));
		}

		PListType threadsFinished = 0;
		while (threadsFinished != threadsToDispatch)
		{
			for (PListType k = 0; k < threadsToDispatch; k++)
			{
				if (threadPool != NULL)
				{
					(*threadPool)[k].get();
					threadsFinished++;
				}
			}
		}
	}
	else
	{
		
		vector<vector<PListType>> balancedTruncList = ProcessThreadsWorkLoad(threadsToDispatch, prevPListArray);
		vector<unsigned int> localWorkingThreads;
		for(unsigned int i = 0; i < balancedTruncList.size(); i++)
		{
			activeThreads[i] = true;
			localWorkingThreads.push_back(i);
		}

		countMutex->lock();
		for (PListType i = 0; i < localWorkingThreads.size(); i++)
		{
			LevelPackage levelInfo;
			levelInfo.currLevel = globalLevel;
			levelInfo.threadIndex = i;
			levelInfo.inceptionLevelLOL = 0;
			threadsDispatched++;
			threadPool->push_back(std::async(std::launch::async, &Forest::ThreadedLevelTreeSearchRecursionListRAM, this, prevPListArray, balancedTruncList[i], balancedTruncList[i].size(), levelInfo));
				
		}
		countMutex->unlock();

		WaitForThreads(localWorkingThreads, threadPool);
		
		levelCountPatterns = globalPListArray->size();

		prevPListArray->clear();
		prevPListArray->swap((*globalPListArray));
	}

	if(levelCountPatterns > 0)
	{
		continueSearching = true;
	}
	
	DisplayPatternsFound();

	threadPool->erase(threadPool->begin(), threadPool->end());
	(*threadPool).clear();

	return continueSearching;
}

void Forest::WaitForThreads(vector<unsigned int> localWorkingThreads, vector<future<void>> *localThreadPool, bool recursive)
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
				if (localThreadPool != NULL && (*localThreadPool)[localWorkingThreads[k]].wait_for(std::chrono::milliseconds(100)) == std::future_status::ready)
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
						
						StopWatch time = initTime;
						time.Stop();
						time.Display();
						
						countMutex->lock();
						activeThreads[k] = false;
						countMutex->unlock();

						stringstream buff;
						buff << "Thread " << localWorkingThreads[k] << " finished all processing" << endl;
						Logger::WriteLog(buff.str());
						cout << buff.str();

						cout << "Threads in use " << threadsDispatched - threadsDefuncted << endl;

					}

				}
				else
				{
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

vector<vector<PListType>> Forest::ProcessThreadsWorkLoad(unsigned int threadsToDispatch, vector<vector<PListType>*>* patterns)
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
			//cout << " Pattern size: " << (*patterns)[i]->size() << endl;
			bool found = false;
			PListType smallestIndex = 0;
			PListType smallestAmount = 10000000000000000000;
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
		
		if(i < localWorkingThreads.size())
		{
			cout << "Thread " << i << " is processing " << balancedSizeList[i] << " indexes with " << balancedTruncList[i].size() << " patterns\n";
		}
		else
		{
			cout << "Thread " << i << " is not processing " << balancedSizeList[i] << " indexes with " << balancedTruncList[i].size() << " patterns\n";
		}
	}


	return balancedTruncList;
}

PListType Forest::ProcessChunks(vector<string> fileNamesToReOpen, PListType memDivisor)
{
	int currentFile = 0;
	bool memoryOverflow = false;
	PListType interimCount = 0;

	vector<string> fileNamesBackup;
	for(int a = 0; a < fileNamesToReOpen.size(); a++)
	{
		string newFileName = fileNamesToReOpen[a];
		string patternNameToCopyFrom = newFileName;
		string patternName = newFileName;
		patternName.append("backupPatterns");
		newFileName.append("backup");
		string fileNameage(newFileName);
		fileNameage.insert(0, ARCHIVE_BACKUP_FOLDER);
		fileNameage.append(".txt");
		ofstream outputFile(fileNameage);
		outputFile.close();
		MemoryUtils::copyFileOverBackup(fileNamesToReOpen[a], newFileName);
		fileNamesBackup.push_back(newFileName);

		patternNameToCopyFrom.append("Patterns");

		string fileNameage2(patternName);
		fileNameage2.insert(0, ARCHIVE_BACKUP_FOLDER);
		fileNameage2.append(".txt");
		ofstream outputFile2(fileNameage2);
		outputFile2.close();
		MemoryUtils::copyFileOverBackup(patternNameToCopyFrom, patternName);
	}


	while(currentFile < fileNamesBackup.size())
	{
		memoryOverflow = false;

		vector<PListArchive*> archiveCollection;
		map<PatternType, PListType> finalMetaDataMap;

		for(int a = currentFile; a < fileNamesBackup.size(); a++)
		{
			archiveCollection.push_back(new PListArchive(fileNamesBackup[a], true, true));
		}
			
		for(int a = 0; a < archiveCollection.size(); a++)
		{
			
			vector<vector<PListType>*>* packedPListArray = archiveCollection[a]->GetPListArchiveMMAP(memDivisor/1000000.0f); //GetPListARchiveMMAP takes MB not bytes
			PListType packedPListSize = packedPListArray->size();

			std::string::size_type i = archiveCollection[a]->fileName.find("backup.txt");
			string tempString = archiveCollection[a]->fileName;
			tempString.erase(i, 14);
			tempString.erase(0, 17);
			string fileNameForLater;
			fileNameForLater.append(tempString);
			string fileName(fileNameForLater);
			fileName.append("backupPatterns");
			PListArchive *stringBufferFile = new PListArchive(fileName, true, true);
			vector<string> *stringBuffer = stringBufferFile->GetPatterns(globalLevel, packedPListArray->size());

			if(MemoryUtils::IsOverMemoryCount(MemoryUsedPriorToThread, memoryBandwidthMB) && !memoryOverflow)
			{
				memoryOverflow = true;
				currentFile = a;
			}
			else if(MemoryUtils::IsOverMemoryCount(MemoryUsedPriorToThread, memoryBandwidthMB) && memoryOverflow)
			{
				//wait for one second for other memory to clear up
				std::this_thread::sleep_for (std::chrono::seconds(1));
			}
				
			for(PListType partialLists = 0; partialLists < packedPListArray->size(); partialLists++)
			{
				try
				{
					string pattern = (*stringBuffer)[partialLists];
					//This allows us to back fill the others iterations when this didn't have a pattern
					if(finalMetaDataMap.find(pattern) == finalMetaDataMap.end())
					{
						if(!memoryOverflow)
						{
							finalMetaDataMap[pattern] = (*packedPListArray)[partialLists]->size();
							delete (*packedPListArray)[partialLists];
							(*packedPListArray)[partialLists] = NULL;
						}
					}
					else
					{
						finalMetaDataMap[pattern] += (*packedPListArray)[partialLists]->size();
						delete (*packedPListArray)[partialLists];
						(*packedPListArray)[partialLists] = NULL;
					}
						
				}
				catch(...)
				{
					cout << "System exception: " << endl;
				}
			}

			if(!memoryOverflow)
			{
				currentFile++;
			}
				
			archiveCollection[a]->DumpContents();
			archiveCollection[a]->CloseArchiveMMAP();

			delete archiveCollection[a];

			stringBufferFile->DumpContents();
			stringBufferFile->CloseArchiveMMAP();

			delete stringBufferFile;

			string backupFile = fileNameForLater;
			backupFile.append("backup");
			PListArchive* archiveCollective = new PListArchive(backupFile, true, true);
			for(PListType partialLists = 0; partialLists < packedPListSize; partialLists++)
			{
				if((*packedPListArray)[partialLists] != NULL)
				{
					archiveCollective->WriteArchiveMapMMAP((*packedPListArray)[partialLists], (*stringBuffer)[partialLists]);
					delete (*packedPListArray)[partialLists];
					(*packedPListArray)[partialLists] = NULL;
				}
			}

			//cout << "file name: " << archiveCollective->fileName << " size of new plist buffer: " << pListNewBufferCount << " size of new string buffer: " << archiveCollective->stringBuffer.size() << endl;

			archiveCollective->WriteArchiveMapMMAP(NULL, "", true);
			archiveCollective->CloseArchiveMMAP();

			archiveCollective->DumpPatternsToDisk(globalLevel);

			delete archiveCollective;

			delete packedPListArray;

			stringBuffer->clear();
			delete stringBuffer;
		}

		for(it_map_type iterator = finalMetaDataMap.begin(); iterator != finalMetaDataMap.end(); iterator++)
		{
			if(iterator->second > 1)
			{
				/*stringstream builder;
				builder << "Pattern " << iterator->first << " occurs " << iterator->second << " at level " << globalLevel << endl;
				Logger::WriteLog(builder.str());*/
				interimCount++;
			}
			else
			{
				eradicatedPatterns++;
			}
		}
	}
	countMutex->lock();
	levelCountPatterns += interimCount;
	countMutex->unlock();

	//Delete unused backup files 
	for(int a = 0; a < fileNamesBackup.size(); a++)
	{
		DeleteChunks(fileNamesBackup, ARCHIVE_BACKUP_FOLDER);
	}

	return interimCount;
}

PListType Forest::ProcessChunksAndGenerate(vector<string> fileNamesToReOpen, PListType memDivisor, unsigned int threadNum, bool firstLevel)
{
	int currentFile = 0;
	bool memoryOverflow = false;
	PListType interimCount = 0;
	unsigned int threadNumber = 0;
	unsigned int threadsToDispatch = numThreads - 1;
	
	if(!firstLevel)
	{
		newFileNameList[threadNum].clear();
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
		map<PatternType, vector<PListType>*> finalMetaDataMap;

		for(int a = currentFile; a < fileNamesBackup.size(); a++)
		{
			archiveCollection.push_back(new PListArchive(fileNamesBackup[a], true, false));
		}
			
		for(int a = 0; a < archiveCollection.size(); a++)
		{
			
			//Our job is to trust whoever made the previous chunk made it within the acceptable margin of error so we compensate by saying take up to double the size if the previous
			//chunk went a little over the allocation constraint
			cout << "Processing file : " << archiveCollection[a]->fileName << endl;
			vector<vector<PListType>*>* packedPListArray = archiveCollection[a]->GetPListArchiveMMAP(/*2.0f*memDivisor/1000000.0*/); //Needs MB
			PListType packedPListSize = packedPListArray->size();

			std::string::size_type i = archiveCollection[a]->fileName.find(".txt");
			string tempString = archiveCollection[a]->fileName;
			tempString.erase(i, 8);
			tempString.erase(0, 7);
			string fileNameForLater;
			fileNameForLater.append(tempString);
			string fileName(fileNameForLater);
			fileName.append("Patterns");
			PListArchive *stringBufferFile = new PListArchive(fileName, true, false);
			vector<string> *stringBuffer = stringBufferFile->GetPatterns(globalLevel, packedPListArray->size());

			if(MemoryUtils::IsOverMemoryCount(MemoryUsedPriorToThread, memoryBandwidthMB) && !memoryOverflow)
			{
				memoryOverflow = true;
			}
			else if(MemoryUtils::IsOverMemoryCount(MemoryUsedPriorToThread, memoryBandwidthMB) && memoryOverflow)
			{
				//wait for one second for other memory to clear up
				std::this_thread::sleep_for (std::chrono::seconds(1));
			}
				
			for(PListType partialLists = 0; partialLists < packedPListArray->size(); partialLists++)
			{
				try
				{
					string pattern = (*stringBuffer)[partialLists];
					//This allows us to back fill the others iterations when this didn't have a pattern
					if(finalMetaDataMap.find(pattern) == finalMetaDataMap.end())
					{
						if(!memoryOverflow)
						{
							finalMetaDataMap[pattern] = new vector<PListType>();
							finalMetaDataMap[pattern]->insert(finalMetaDataMap[pattern]->end(), (*packedPListArray)[partialLists]->begin(), (*packedPListArray)[partialLists]->end());
							delete (*packedPListArray)[partialLists];
							(*packedPListArray)[partialLists] = NULL;
						}
					}
					else
					{
						finalMetaDataMap[pattern]->insert(finalMetaDataMap[pattern]->end(), (*packedPListArray)[partialLists]->begin(), (*packedPListArray)[partialLists]->end());
						delete (*packedPListArray)[partialLists];
						(*packedPListArray)[partialLists] = NULL;
					}
						
				}
				catch(exception e)
				{
					cout << "System exception: " << e.what() << endl;
				}
			}

			if(!memoryOverflow)
			{
				currentFile++;
			}
				
			archiveCollection[a]->DumpContents();
			archiveCollection[a]->CloseArchiveMMAP();

			delete archiveCollection[a];

			stringBufferFile->DumpContents();
			stringBufferFile->CloseArchiveMMAP();

			delete stringBufferFile;

			string backupFile = fileNameForLater;
			PListArchive* archiveCollective = new PListArchive(backupFile, true, false);
			for(PListType partialLists = 0; partialLists < packedPListSize; partialLists++)
			{
				if((*packedPListArray)[partialLists] != NULL)
				{
					archiveCollective->WriteArchiveMapMMAP((*packedPListArray)[partialLists], (*stringBuffer)[partialLists]);
					delete (*packedPListArray)[partialLists];
					(*packedPListArray)[partialLists] = NULL;
				}
			}

			//cout << "file name: " << archiveCollective->fileName << " size of new plist buffer: " << pListNewBufferCount << " size of new string buffer: " << archiveCollective->stringBuffer.size() << endl;

			archiveCollective->WriteArchiveMapMMAP(NULL, "", true);
			archiveCollective->CloseArchiveMMAP();

			archiveCollective->DumpPatternsToDisk(globalLevel);

			delete archiveCollective;

			delete packedPListArray;

			stringBuffer->clear();
			delete stringBuffer;
		}

		PListType tempMemoryMB = MemoryUtils::GetProgramMemoryConsumption();
		PListType memDivisorMB = memDivisor/1000000.0f;

		//thread files
		PListArchive* currChunkFile = NULL;
		bool notBegun = true;
		PListType removedPatterns = 0;

		for(it_map_list_p_type iterator = finalMetaDataMap.begin(); iterator != finalMetaDataMap.end(); iterator++)
		{
			if(MemoryUtils::IsLessThanMemoryCount(tempMemoryMB, memDivisorMB) || notBegun)
			{
				notBegun = false;
				if(currChunkFile != NULL)
				{
					currChunkFile->WriteArchiveMapMMAP(NULL, "", true);
					currChunkFile->CloseArchiveMMAP();
					delete currChunkFile;
				}
				
				if(firstLevel)
				{
					stringstream fileNameage;
					string formattedTime = Logger::GetFormattedTime();
					fileNameage << ARCHIVE_FOLDER << "PListArchive" << threadNumber << formattedTime << ".txt";
					ofstream outputFile(fileNameage.str());
					outputFile.close();
	
					stringstream fileNameForPrevList;
					fileNameForPrevList << "PListArchive" << threadNumber << formattedTime;
					prevFileNameList[threadNumber].push_back(fileNameForPrevList.str());
				
					currChunkFile = new PListArchive(fileNameForPrevList.str());
					//After dumping file to memory lets get a new curr memory
					tempMemoryMB = MemoryUtils::GetProgramMemoryConsumption();
	
					threadNumber++;
					threadNumber %= threadsToDispatch;
				}
				else
				{
					stringstream fileNameage;
					string formattedTime = Logger::GetFormattedTime();
					fileNameage << ARCHIVE_FOLDER << "PListArchive" << threadNum << formattedTime << ".txt";
					ofstream outputFile(fileNameage.str());
					outputFile.close();
	
					stringstream fileNameForPrevList;
					fileNameForPrevList << "PListArchive" << threadNum << formattedTime;
					newFileNameList[threadNum].push_back(fileNameForPrevList.str());
				
					currChunkFile = new PListArchive(fileNameForPrevList.str());
					//After dumping file to memory lets get a new curr memory
					tempMemoryMB = MemoryUtils::GetProgramMemoryConsumption();
				}
			}
			
			if(iterator->second->size() > 1)
			{
				currChunkFile->WriteArchiveMapMMAP(iterator->second);
				interimCount++;
				
				stringstream builder;
				builder << "Pattern " << (*iterator).first << " occurs " << iterator->second->size() << endl;
				Logger::WriteLog(builder.str());
			}
			else
			{
				removedPatterns++;
			}
			
			delete iterator->second;
		}

		if(currChunkFile != NULL)
		{
			currChunkFile->WriteArchiveMapMMAP(NULL, "", true);
			currChunkFile->CloseArchiveMMAP();
			delete currChunkFile;
		}
		
		countMutex->lock();
		eradicatedPatterns += removedPatterns;
		countMutex->unlock();

		//cout << "Thread " << threadNum << " has encountered " << interimCount << " patterns!" << endl;
	}

	countMutex->lock();
	levelCountPatterns += interimCount;
	countMutex->unlock();

	//Delete unused backup files 
	for(int a = 0; a < fileNamesBackup.size(); a++)
	{
		DeleteChunks(fileNamesBackup, ARCHIVE_BACKUP_FOLDER);
	}

	
	return interimCount;
}

void Forest::ThreadedLevelTreeSearch(PListType threadNum)
{
	//Divide between file load and previous level pLists and leave some for new lists haha 
	unsigned long long memDivisor = (memoryPerThread*1000000)/3.0f;
		
	bool morePatternsToFind = false;

	PListArchive *fileArchive = new PListArchive(file->fileName, false);
	unsigned long long localFileSize = fileArchive->GetFileChunkSize(memDivisor + globalLevel);
	fileArchive->CloseArchiveMMAP();
	delete fileArchive;

	unsigned long long fileIters = file->fileStringSize/localFileSize;
	if(file->fileStringSize%localFileSize != 0)
	{
		fileIters++;
	}

	typedef std::vector<map<PatternType, PListType>>::iterator it_type;
	typedef std::map<PatternType, PListType>::iterator it_map_type;
	typedef std::map<PatternType, vector<PListType>>::iterator it_map_list_type;

	string fileChunkData;
	unsigned long long fileIterationLevel = 0;
	vector<string> fileNamesToReOpen;
	string saveOffPreviousStringData = "";

	try
	{
		for(PListType prevChunkCount = 0; prevChunkCount < prevFileNameList[threadNum].size(); prevChunkCount++)
		{
			PListArchive archive(prevFileNameList[threadNum][prevChunkCount]);
			
			while(!archive.IsEndOfFile())
			{
				vector<vector<PListType>*>* packedPListArray = archive.GetPListArchiveMMAP(memDivisor/1000000.0f); //Needs MB
				PListType packedListSize = packedPListArray->size();
				vector <PListType> prevLeafIndex(packedListSize, 0);
		
				for(PListType j = 0; j < fileIters; j++)
				{
					if(packedListSize > 0)
					{
						PListArchive *fileArchive = new PListArchive(file->fileName, false);
						if(fileChunkData.size() > 0)
						{
							saveOffPreviousStringData = fileChunkData.substr(fileChunkData.size() - (globalLevel - 1), globalLevel - 1);
						}
						fileChunkData.clear();
						fileChunkData = "";
						fileChunkData = fileArchive->GetFileChunk(j*memDivisor, memDivisor + globalLevel);
						fileArchive->CloseArchiveMMAP();
						delete fileArchive;
					}

					unsigned int internalCount = 0;
					TreeHD leaf;

					bool justPassedMemorySize = false;

					for(PListType i = 0; i < packedListSize; i++)
					{
						vector<PListType>* pList = (*packedPListArray)[i];
						PListType pListLength = (*packedPListArray)[i]->size();
						PListType k = prevLeafIndex[i];

						if(!MemoryUtils::IsOverMemoryCount(MemoryUsedPriorToThread, memoryBandwidthMB))
						{
							while( k < pListLength && ((*pList)[k]) < (j+1)*localFileSize )
							{
								try
								{
									if(((*pList)[k]) < file->fileStringSize)
									{
										//If index comes out to be larger than fileString than it is a negative number 
										//and we must use previous string data!
										if(((((*pList)[k])%localFileSize) - (globalLevel-1)) >= file->fileStringSize)
										{
											signed long long relativeIndex = ((((*pList)[k])%localFileSize) - (globalLevel-1));
											relativeIndex *= -1;
											PListType indexForString = saveOffPreviousStringData.size() - relativeIndex;
											string pattern = saveOffPreviousStringData.substr(indexForString, relativeIndex);
											pattern.append(fileChunkData.substr(0, globalLevel - pattern.size()));
											leaf.addLeaf((*pList)[k]+1, pattern);
											cout << "string over the border: " << saveOffPreviousStringData << endl;
											cout << "Relative index: " << relativeIndex << " Index for string: " << indexForString << " pattern: " << pattern << " is size: " << pattern.size() << endl;
										}
										else
										{
											//If pattern is past end of string stream then stop counting this pattern
											if(((*pList)[k]) < file->fileStringSize)
											{
												leaf.addLeaf((*pList)[k]+1, fileChunkData.substr(((((*pList)[k])%localFileSize) - (globalLevel-1)), globalLevel));
											}
											else if(((((*pList)[k])%localFileSize) - (globalLevel-1)) < 0)
											{
												cout << "String access is out of bounds at beginning" << endl;
											}
											else if((((*pList)[k])%localFileSize) >= file->fileStringSize)
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
									cout << "Exception at global index: " << (*pList)[k] << "Exception at relative index: " << ((((*pList)[k])%localFileSize) - (globalLevel-1)) << "    System exception: " << e.what() << endl;
								}
								k++;
							}
							prevLeafIndex[i] = k;
							justPassedMemorySize = false;
						}
						else
						{
							//if true already do not write again until memory is back in our hands
							if(!justPassedMemorySize)
							{
								
								cout << "Prior memory to dump: " << MemoryUtils::GetProgramMemoryConsumption() << endl;

								PListType patterns = leaf.leaves.size();
								PListType sizeOfTreeMap = sizeof(char)*globalLevel*patterns + sizeof(TreeHD)*patterns;
								PListType currentTreeSizeMB = (patterns*sizeof(PListType) + sizeOfTreeMap)/1000000.0f;
								sizeOfPreviousLevelMB += currentTreeSizeMB;
								
								justPassedMemorySize = true;
								string formattedTime = Logger::GetFormattedTime();
								stringstream stringBuilder;
								stringBuilder << threadNum << j << fileIterationLevel << internalCount << formattedTime;
								fileNamesToReOpen.push_back(CreateChunkFile(stringBuilder.str(), leaf, threadNum));
								internalCount++;

								cout << "Memory after dump: " << MemoryUtils::GetProgramMemoryConsumption() << endl;
								
							}
							else
							{
								
								//If memory is unavailable sleep for one second
								std::this_thread::sleep_for (std::chrono::seconds(1));
							}
							i--;
						}
					}

					cout << "Prior memory to dump: " << MemoryUtils::GetProgramMemoryConsumption() << endl;

					PListType patterns = leaf.leaves.size();
					PListType sizeOfTreeMap = sizeof(char)*globalLevel*patterns + sizeof(TreeHD)*patterns;
					PListType currentTreeSizeMB = (patterns*sizeof(PListType) + sizeOfTreeMap)/1000000.0f;
					//cout << "Dumping off a leaf load of " << patterns << " patterns at level " << globalLevel << endl;
					sizeOfPreviousLevelMB += currentTreeSizeMB;

					string formattedTime = Logger::GetFormattedTime();
					stringstream stringBuilder;
					stringBuilder << threadNum << j << fileIterationLevel << internalCount << formattedTime;
					fileNamesToReOpen.push_back(CreateChunkFile(stringBuilder.str(), leaf, threadNum));
					delete leaf.GetPList();
					internalCount++;

					cout << "Memory after dump: " << MemoryUtils::GetProgramMemoryConsumption() << endl;
				}
		
				for(PListType pTits = 0; pTits < packedPListArray->size(); pTits++)
				{
					delete (*packedPListArray)[pTits];
				}
				delete packedPListArray;

				fileIterationLevel++;
				
				
			}
			archive.CloseArchiveMMAP();
		}

		fileChunkData.clear();
		fileChunkData = "";

		if(levelToOutput == 0 || (levelToOutput != 0 && globalLevel >= levelToOutput))
		{
			ProcessChunksAndGenerate(fileNamesToReOpen, memDivisor, threadNum);
		}
	}
	catch(exception e)
	{
		cout << e.what() << endl;
		MemoryUtils::print_trace();
	}

	if(!history)
	{
		DeleteChunks(prevFileNameList[threadNum], ARCHIVE_FOLDER);
	}

	prevFileNameList[threadNum].clear();
	for(int i = 0; i < newFileNameList[threadNum].size(); i++)
	{
		string fileNameToBeRemoved = ARCHIVE_FOLDER;
		fileNameToBeRemoved.append(newFileNameList[threadNum][i].c_str());
		fileNameToBeRemoved.append(".txt");
		
		PListType fileSizeOfChunk = MemoryUtils::FileSize(fileNameToBeRemoved);
		if(fileSizeOfChunk > 0)
		{
			prevFileNameList[threadNum].push_back(newFileNameList[threadNum][i]);
		}
		else
		{
			if( remove( fileNameToBeRemoved.c_str() ) != 0 )
			{
				cout << "Failed to delete '" << fileNameToBeRemoved << "': " << strerror(errno) << '\n';
			}
			else
			{
				//cout << fileNameToBeRemoved << " successfully deleted" << endl;
			}
		}
	}
	
	newFileNameList[threadNum].clear();

	return;
}

void Forest::ThreadedLevelTreeSearchRecursionListHD(vector<vector<PListType>*>* patterns, vector<PListType> patternIndexList, PListType numPatternsToSearch, LevelPackage levelInfo)
{
	bool isThreadDefuncted = false;
	cout << "Threads dispatched: " << threadsDispatched << " Threads deported: " << threadsDefuncted << " Threads running: " << threadsDispatched - threadsDefuncted << endl;
	
	int tempCurrentLevel = levelInfo.currLevel;
	int threadsToDispatch = numThreads - 1;

	if(threadsDispatched - threadsDefuncted > threadsToDispatch)
	{
		cout << "WENT OVER THREADS ALLOCATION SIZE!" << endl;
	}
	
	vector<vector<PListType>*>* prevLocalPListArray = new vector<vector<PListType>*>();
	for(PListType i = 0; i < numPatternsToSearch; i++)
	{
		if((*patterns)[patternIndexList[i]] != NULL)
		{
			prevLocalPListArray->push_back((*patterns)[patternIndexList[i]]);
		}
	}
	bool continueSearching = true;
	vector<vector<PListType>*>* globalLocalPListArray = new vector<vector<PListType>*>();

	while(continueSearching)
	{
		for (PListType i = 0; i < prevLocalPListArray->size(); i++)
		{

			//used primarily for just storage containment
			TreeRAM leaf;

			vector<PListType>* pList = (*prevLocalPListArray)[i];
			PListType pListLength = (*prevLocalPListArray)[i]->size();

			for (PListType k = 0; k < pListLength; k++)
			{
				//If pattern is past end of string stream then stop counting this pattern
				if ((*pList)[k] < file->fileStringSize)
				{
					unsigned char value = file->fileString[(*pList)[k]];
					leaf.addLeaf(value, (*pList)[k] + 1);
				}
			}
		
			PListType removedPatternsTemp = 0;
			vector<vector<PListType>*>* newList = leaf.GetLeafPLists(removedPatternsTemp);

			if(newList != NULL)
			{
				globalLocalPListArray->insert(globalLocalPListArray->end(), newList->begin(), newList->end());
				newList->erase(newList->begin(), newList->end());
				delete newList;
			}










			//if(!MemoryUtils::IsOverMemoryCount(MemoryUsedPriorToThread, memoryBandwidthMB))
			//{
			//	while( k < pListLength && ((*pList)[k]) < (j+1)*localFileSize )
			//	{
			//		try
			//		{
			//			if(((*pList)[k]) < file->fileStringSize)
			//			{
			//				//If index comes out to be larger than fileString than it is a negative number 
			//				//and we must use previous string data!
			//				if(((((*pList)[k])%localFileSize) - (globalLevel-1)) >= file->fileStringSize)
			//				{
			//					signed long long relativeIndex = ((((*pList)[k])%localFileSize) - (globalLevel-1));
			//					relativeIndex *= -1;
			//					PListType indexForString = saveOffPreviousStringData.size() - relativeIndex;
			//					string pattern = saveOffPreviousStringData.substr(indexForString, relativeIndex);
			//					pattern.append(fileChunkData.substr(0, globalLevel - pattern.size()));
			//					leaf.addLeaf((*pList)[k]+1, pattern);
			//					cout << "string over the border: " << saveOffPreviousStringData << endl;
			//					cout << "Relative index: " << relativeIndex << " Index for string: " << indexForString << " pattern: " << pattern << " is size: " << pattern.size() << endl;
			//				}
			//				else
			//				{
			//					//If pattern is past end of string stream then stop counting this pattern
			//					if(((*pList)[k]) < file->fileStringSize)
			//					{
			//						leaf.addLeaf((*pList)[k]+1, fileChunkData.substr(((((*pList)[k])%localFileSize) - (globalLevel-1)), globalLevel));
			//					}
			//					else if(((((*pList)[k])%localFileSize) - (globalLevel-1)) < 0)
			//					{
			//						cout << "String access is out of bounds at beginning" << endl;
			//					}
			//					else if((((*pList)[k])%localFileSize) >= file->fileStringSize)
			//					{
			//						cout << "String access is out of bounds at end" << endl;
			//					}
			//				}
			//			}
			//			else
			//			{
			//				cout << "don't pattern bro at this index: " << ((*pList)[k]) << endl;
			//			}
			//		}
			//		catch(exception e)
			//		{
			//			cout << "Exception at global index: " << (*pList)[k] << "Exception at relative index: " << ((((*pList)[k])%localFileSize) - (globalLevel-1)) << "    System exception: " << e.what() << endl;
			//		}
			//		k++;
			//	}
			//	prevLeafIndex[i] = k;
			//	justPassedMemorySize = false;
			//}
			//else
			//{
			//	//if true already do not write again until memory is back in our hands
			//	if(!justPassedMemorySize)
			//	{
			//					
			//		cout << "Prior memory to dump: " << MemoryUtils::GetProgramMemoryConsumption() << endl;

			//		PListType patterns = leaf.leaves.size();
			//		PListType sizeOfTreeMap = sizeof(char)*globalLevel*patterns + sizeof(TreeHD)*patterns;
			//		PListType currentTreeSizeMB = (patterns*sizeof(PListType) + sizeOfTreeMap)/1000000.0f;
			//		sizeOfPreviousLevelMB += currentTreeSizeMB;
			//					
			//		justPassedMemorySize = true;
			//		string formattedTime = Logger::GetFormattedTime();
			//		stringstream stringBuilder;
			//		stringBuilder << threadNum << j << fileIterationLevel << internalCount << formattedTime;
			//		fileNamesToReOpen.push_back(CreateChunkFile(stringBuilder.str(), leaf, threadNum));
			//		internalCount++;

			//		cout << "Memory after dump: " << MemoryUtils::GetProgramMemoryConsumption() << endl;
			//					
			//	}
			//	else
			//	{
			//					
			//		//If memory is unavailable sleep for one second
			//		std::this_thread::sleep_for (std::chrono::seconds(1));
			//	}
			//	i--;
			//}









		
			delete leaf.GetPList();

			delete (*prevLocalPListArray)[i];
		}

		if(globalLocalPListArray->size() == 0)
		{
			continueSearching = false;
		}
		else
		{
			countMutex->lock();

			levelRecordings[tempCurrentLevel] += globalLocalPListArray->size();
			tempCurrentLevel++;

			if(tempCurrentLevel > currentLevelVector[levelInfo.threadIndex])
			{
				currentLevelVector[levelInfo.threadIndex] = tempCurrentLevel;
			}
			countMutex->unlock();
		}
		
		prevLocalPListArray->clear();
		prevLocalPListArray->swap((*globalLocalPListArray));

		
		
		
		//Try to spawn thread within thread

		bool alreadyUnlocked = false;
		countMutex->lock();

		int unusedCores = (threadsToDispatch - (threadsDispatched - threadsDefuncted)) + 1;
		if(prevLocalPListArray->size() < unusedCores && unusedCores > 1)
		{
			unusedCores = prevLocalPListArray->size();
		}
		//Need to have an available core, need to still have patterns to search and need to have more than 1 pattern to be worth splitting up the work
		if(unusedCores > 1 && continueSearching && prevLocalPListArray->size() > 1)
		{
			//cout << "Cores not being used: " << unusedCores << endl;
			unsigned int levelCount = 1000000000;
			vector<int> threadPriority;

			for(int z = 0; z < currentLevelVector.size(); z++)
			{
				if(activeThreads[z])
				{
					if(currentLevelVector[z] < levelCount && activeThreads[z])
					{
						levelCount = currentLevelVector[z];
						threadPriority.push_back(z);
					}
				}
			}

			bool spawnThreads = false;
			for(int z = 0; z < threadPriority.size(); z++)
			{
				//cout << "Thread " << threadPriority[z] << " is at level " << currentLevelVector[threadPriority[z]] << endl;
				if(threadPriority[z] == levelInfo.threadIndex && currentLevelVector[threadPriority[z]] == levelCount)
				{
					spawnThreads = true;
				}
			}
			//If this thread is at the lowest level of progress spawn new threads
			if(spawnThreads)
			{
			
				vector<vector<PListType>> balancedTruncList = ProcessThreadsWorkLoad(unusedCores, prevLocalPListArray);
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
								tally += (*prevLocalPListArray)[balancedTruncList[z][d]]->size();
							}
							//cout << "Thread number " << levelInfo.threadIndex << " at sub thread " << z << " is processing a list of size " << balancedTruncList[z].size() << " with " << tally  << " pLists" << endl;
						}
						cout << "Thread " << levelInfo.threadIndex << " has priority and is at level " << tempCurrentLevel << endl;

						LevelPackage levelInfoRecursion;
						levelInfoRecursion.currLevel = tempCurrentLevel;
						levelInfoRecursion.threadIndex = levelInfo.threadIndex;
						levelInfoRecursion.inceptionLevelLOL = levelInfo.inceptionLevelLOL + 1;
/*
						cout << "Threads to be dispatched: " << localWorkingThreads.size() << endl;
						cout << "Thread: " << levelInfoRecursion.threadIndex << endl;
						cout << "Inception level: " << levelInfoRecursion.inceptionLevelLOL << endl;*/
						cout << "Current threads in use: " << threadsDispatched - threadsDefuncted + localWorkingThreads.size() - 1 << endl;

						threadsDefuncted++;
						isThreadDefuncted = true;

						vector<future<void>> *localThreadPool = new vector<future<void>>();
						for (PListType i = 0; i < localWorkingThreads.size(); i++)
						{
							threadsDispatched++;
							localThreadPool->push_back(std::async(std::launch::async, &Forest::ThreadedLevelTreeSearchRecursionListRAM, this, prevLocalPListArray, balancedTruncList[i], balancedTruncList[i].size(), levelInfoRecursion));
						}
						countMutex->unlock();
						
						alreadyUnlocked = true;
						WaitForThreads(localWorkingThreads, localThreadPool, true);

						localThreadPool->erase(localThreadPool->begin(), localThreadPool->end());
						(*localThreadPool).clear();
						delete localThreadPool;
						continueSearching = false;
					}
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

void Forest::ThreadedLevelTreeSearchRecursionListRAM(vector<vector<PListType>*>* patterns, vector<PListType> patternIndexList, PListType numPatternsToSearch, LevelPackage levelInfo)
{

	bool isThreadDefuncted = false;
	cout << "Threads dispatched: " << threadsDispatched << " Threads deported: " << threadsDefuncted << " Threads running: " << threadsDispatched - threadsDefuncted << endl;
	
	int tempCurrentLevel = levelInfo.currLevel;
	int threadsToDispatch = numThreads - 1;

	if(threadsDispatched - threadsDefuncted > threadsToDispatch)
	{
		cout << "WENT OVER THREADS ALLOCATION SIZE!" << endl;
	}
	
	vector<vector<PListType>*>* prevLocalPListArray = new vector<vector<PListType>*>();
	for(PListType i = 0; i < numPatternsToSearch; i++)
	{
		if((*patterns)[patternIndexList[i]] != NULL)
		{
			prevLocalPListArray->push_back((*patterns)[patternIndexList[i]]);
		}
	}
	bool continueSearching = true;
	vector<vector<PListType>*>* globalLocalPListArray = new vector<vector<PListType>*>();

	while(continueSearching)
	{
		//PListType removedPatterns = 0;

		for (PListType i = 0; i < prevLocalPListArray->size(); i++)
		{

			//used primarily for just storage containment
			TreeRAM leaf;

			vector<PListType>* pList = (*prevLocalPListArray)[i];
			PListType pListLength = (*prevLocalPListArray)[i]->size();

			for (PListType k = 0; k < pListLength; k++)
			{
				//If pattern is past end of string stream then stop counting this pattern
				if ((*pList)[k] < file->fileStringSize)
				{
					unsigned char value = file->fileString[(*pList)[k]];
					leaf.addLeaf(value, (*pList)[k] + 1);
				}
			}
		
			PListType removedPatternsTemp = 0;
			vector<vector<PListType>*>* newList = leaf.GetLeafPLists(removedPatternsTemp);

			if(newList != NULL)
			{
				globalLocalPListArray->insert(globalLocalPListArray->end(), newList->begin(), newList->end());
				newList->erase(newList->begin(), newList->end());
				delete newList;
			}
		
			delete leaf.GetPList();

			delete (*prevLocalPListArray)[i];
		}

		if(globalLocalPListArray->size() == 0)
		{
			continueSearching = false;
		}
		else
		{
			countMutex->lock();
			////double size if exceeded
			//if(tempCurrentLevel >= levelRecordings.size())
			//{
			//	levelRecordings.resize(levelRecordings.size()*2);
			//}

			levelRecordings[tempCurrentLevel] += globalLocalPListArray->size();
			tempCurrentLevel++;

			if(tempCurrentLevel > currentLevelVector[levelInfo.threadIndex])
			{
				currentLevelVector[levelInfo.threadIndex] = tempCurrentLevel;
			}
			countMutex->unlock();
		}
		
		prevLocalPListArray->clear();
		prevLocalPListArray->swap((*globalLocalPListArray));
		
		/*countMutex->lock();
		eradicatedPatterns += removedPatterns;
		countMutex->unlock();*/
		//Try to spawn thread within thread

		
#if THREAD_SPAWNING

		
		bool alreadyUnlocked = false;
		countMutex->lock();

		int unusedCores = (threadsToDispatch - (threadsDispatched - threadsDefuncted)) + 1;
		if(prevLocalPListArray->size() < unusedCores && unusedCores > 1)
		{
			unusedCores = prevLocalPListArray->size();
		}
		//Need to have an available core, need to still have patterns to search and need to have more than 1 pattern to be worth splitting up the work
		if(unusedCores > 1 && continueSearching && prevLocalPListArray->size() > 1)
		{
			//cout << "Cores not being used: " << unusedCores << endl;
			unsigned int levelCount = 1000000000;
			vector<int> threadPriority;

			for(int z = 0; z < currentLevelVector.size(); z++)
			{
				if(activeThreads[z])
				{
					if(currentLevelVector[z] < levelCount && activeThreads[z])
					{
						levelCount = currentLevelVector[z];
						threadPriority.push_back(z);
					}
				}
			}

			bool spawnThreads = false;
			for(int z = 0; z < threadPriority.size(); z++)
			{
				//cout << "Thread " << threadPriority[z] << " is at level " << currentLevelVector[threadPriority[z]] << endl;
				if(threadPriority[z] == levelInfo.threadIndex && currentLevelVector[threadPriority[z]] == levelCount)
				{
					spawnThreads = true;
				}
			}
			//If this thread is at the lowest level of progress spawn new threads
			if(spawnThreads)
			{
			
				vector<vector<PListType>> balancedTruncList = ProcessThreadsWorkLoad(unusedCores, prevLocalPListArray);
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
								tally += (*prevLocalPListArray)[balancedTruncList[z][d]]->size();
							}
							//cout << "Thread number " << levelInfo.threadIndex << " at sub thread " << z << " is processing a list of size " << balancedTruncList[z].size() << " with " << tally  << " pLists" << endl;
						}
						cout << "Thread " << levelInfo.threadIndex << " has priority and is at level " << tempCurrentLevel << endl;

						LevelPackage levelInfoRecursion;
						levelInfoRecursion.currLevel = tempCurrentLevel;
						levelInfoRecursion.threadIndex = levelInfo.threadIndex;
						levelInfoRecursion.inceptionLevelLOL = levelInfo.inceptionLevelLOL + 1;
/*
						cout << "Threads to be dispatched: " << localWorkingThreads.size() << endl;
						cout << "Thread: " << levelInfoRecursion.threadIndex << endl;
						cout << "Inception level: " << levelInfoRecursion.inceptionLevelLOL << endl;*/
						cout << "Current threads in use: " << threadsDispatched - threadsDefuncted + localWorkingThreads.size() - 1 << endl;

						threadsDefuncted++;
						isThreadDefuncted = true;

						vector<future<void>> *localThreadPool = new vector<future<void>>();
						for (PListType i = 0; i < localWorkingThreads.size(); i++)
						{
							threadsDispatched++;
							localThreadPool->push_back(std::async(std::launch::async, &Forest::ThreadedLevelTreeSearchRecursionListRAM, this, prevLocalPListArray, balancedTruncList[i], balancedTruncList[i].size(), levelInfoRecursion));
						}
						countMutex->unlock();
						
						alreadyUnlocked = true;
						WaitForThreads(localWorkingThreads, localThreadPool, true);

						localThreadPool->erase(localThreadPool->begin(), localThreadPool->end());
						(*localThreadPool).clear();
						delete localThreadPool;
						continueSearching = false;
					}
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
#endif // THREAD_SPAWNING
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


TreeHD* Forest::PlantTreeSeedThreadHD(PListType positionInFile, PListType startPatternIndex, PListType numPatternsToSearch, unsigned int threadNum)
{
	PListType memDivisorInMB = (memoryPerThread/3.0f);
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
		leaf.addLeaf(i+positionInFile+1, string(1,file->fileString[i]));
		
		if(MemoryUtils::IsOverMemoryCount(MemoryUsedPriorToThread, /*memDivisorInMB*/memoryBandwidthMB/3.0f))
		{
			string formattedTime = Logger::GetFormattedTime();
			stringstream stringBuilder;
			stringBuilder << threadNum << formattedTime;
			countMutex->lock();
			CreateChunkFile(stringBuilder.str(), leaf, threadNum);
			countMutex->unlock();
		}
#endif
	}

	
	string formattedTime = Logger::GetFormattedTime();
	stringstream stringBuilder;
	stringBuilder << threadNum << formattedTime;
	countMutex->lock();
	CreateChunkFile(stringBuilder.str(), leaf, threadNum);
	countMutex->unlock();
	
	
	delete leaf.GetPList();

	return NULL;
}

TreeRAM* Forest::PlantTreeSeedThreadRAM(PListType positionInFile, PListType startPatternIndex, PListType numPatternsToSearch)
{
	//used primarily for just storage containment
	TreeRAM *leaf = new TreeRAM();
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
		leaf->addLeaf(file->fileString[i], i+positionInFile+1);
#endif
	}


	vector<unsigned char> listToDos;

	typedef std::map<char, TreeRAM*>::iterator it_type;
	std::map<char, TreeRAM*> mapping = leaf->GetMap();
	for(it_type iterator = mapping.begin(); iterator != mapping.end(); iterator++)
	{
		
		vector<PListType>* pList = iterator->second->GetPList();
		unsigned char index = ((unsigned char)iterator->first);
		if((*prevPListArray)[index] == NULL)
		{
			if(gatedMutexes[index]->try_lock())
			{
				(*prevPListArray)[index] = pList;
				gatedMutexes[index]->unlock();
			}
			else
			{
				listToDos.push_back(index);
			}
		}
		else
		{
			if(gatedMutexes[index]->try_lock())
			{
				(*prevPListArray)[index]->insert((*prevPListArray)[index]->end(), pList->begin(), pList->end());
				gatedMutexes[index]->unlock();
				delete pList;
			}
			else
			{
				listToDos.push_back(index);
			}
		}

	}
	while(listToDos.size() > 0)
	{
		vector<unsigned char> nextListToDos;
		for(unsigned char i = 0; i < listToDos.size(); i++)
		{
		
			vector<PListType>* pList = mapping.find(listToDos[i])->second->GetPList();
			if((*prevPListArray)[listToDos[i]] == NULL)
			{
				if(gatedMutexes[listToDos[i]]->try_lock())
				{
					(*prevPListArray)[listToDos[i]] = pList;
					gatedMutexes[listToDos[i]]->unlock();
				}
				else
				{
					nextListToDos.push_back(listToDos[i]);
				}
			}
			else
			{
				if(gatedMutexes[listToDos[i]]->try_lock())
				{
					(*prevPListArray)[listToDos[i]]->insert((*prevPListArray)[listToDos[i]]->end(), pList->begin(), pList->end());
					gatedMutexes[listToDos[i]]->unlock();
					delete pList;
				}
				else
				{
					nextListToDos.push_back(listToDos[i]);
				}
			}
		}
		listToDos.clear();
		listToDos.resize(nextListToDos.size());
		for(int k = 0; k < nextListToDos.size(); k++)
		{
			listToDos[k] = nextListToDos[k];
		}
	}

	delete leaf->GetPList();
	delete leaf;
	return NULL;
}

TreeRAM* Forest::PlantTreeSeedThreadDynamicRAM(PListType positionInFile, PListType startPatternIndex, PListType numPatternsToSearch, PListType levelStart)
{
	//used primarily for just storage containment
	TreeRAMExperiment *leaf = new TreeRAMExperiment();
	for (PListType i = startPatternIndex; i < numPatternsToSearch + startPatternIndex; i++)
	{

#ifdef BYTES
		if(i + levelStart < file->fileString.size())
		{
			leaf->addLeaf(file->fileString.substr(i, levelStart), i+positionInFile+1);
		}
#endif
	}


	vector<unsigned char> listToDos;

	typedef std::map<string, TreeRAMExperiment*>::iterator it_type;
	std::map<string, TreeRAMExperiment*> mapping = leaf->GetMap();
	for(it_type iterator = mapping.begin(); iterator != mapping.end(); iterator++)
	{
		
		vector<PListType>* pList = iterator->second->GetPList();
		
		if(globalMap.find(iterator->first) == globalMap.end())
		{
			globalMap[iterator->first] = pList;
		}
		else
		{
			globalMap[iterator->first]->insert(globalMap[iterator->first]->end(), pList->begin(), pList->end());
		}

	}
	
	

	delete leaf->GetPList();
	delete leaf;
	return NULL;
}

string Forest::CreateChunkFile(string fileName, TreeHD& leaf, unsigned int threadNum)
{
	stringstream archiveName;
	string archiveFileType = "PListChunks";
	string fileNameToReOpen;
	if(globalLevel%2 != 0)
	{
		archiveFileType.append("Temp");
	}

	archiveName << ARCHIVE_FOLDER << archiveFileType << fileName << ".txt";
	//cout << "Output file to be created: " << archiveName.str() << endl;
	ofstream outputFile(archiveName.str());
	outputFile.close();

	archiveName.str("");
	
	archiveName << archiveFileType << fileName;
	
	PListArchive* archiveCollective = new PListArchive(archiveName.str());
	fileNameToReOpen = archiveName.str();
	newFileNameList[threadNum].push_back(archiveName.str());
			
	typedef std::map<string, TreeHD*>::iterator it_type;
	for(it_type iterator = leaf.leaves.begin(); iterator != leaf.leaves.end(); iterator++) 
	{
		vector<PListType>* pList = (*iterator).second->GetPList();
		archiveCollective->WriteArchiveMapMMAP(pList, (*iterator).first);
		/*stringstream builder;
		builder << "Pattern " << (*iterator).first << " occurs " << pList->size() << endl;
		Logger::WriteLog(builder.str());*/
		delete iterator->second->GetPList();
		delete iterator->second;
	}
	leaf.ResetMap();

	archiveCollective->WriteArchiveMapMMAP(NULL, "", true);
	archiveCollective->CloseArchiveMMAP();

	archiveCollective->DumpPatternsToDisk(globalLevel);

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
	
		if( remove( fileNameToBeRemoved.c_str() ) != 0 )
		{
			//cout << "Failed to delete '" << fileNameToBeRemoved << "': " << strerror(errno) << '\n';
		}
		else
		{
			//cout << fileNameToBeRemoved << " successfully deleted" << endl;
		}

		string fileNameToBeRemovedPatterns = folderLocation;
		fileNameToBeRemovedPatterns.append(fileNames[i].c_str());
		fileNameToBeRemovedPatterns.append("Patterns.txt");
	
		if( remove( fileNameToBeRemovedPatterns.c_str() ) != 0 )
		{
			//cout << "Failed to delete '" << fileNameToBeRemovedPatterns << "': " << strerror(errno) << '\n';
		}
		else
		{
			//cout << fileNameToBeRemovedPatterns << " successfully deleted" << endl;
		}
	}
}