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
	system("rm -r ../Log/PList*");
	//mkdir("../Log/BackupLog");

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
	history = false;
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

	memoryCeiling = MemoryUtils::GetAvailableRAMMB() - 1000;
	
	//If memory bandwidth not an input
	if(!usingMemoryBandwidth)
	{
		//Leave 1 GB to spare for operating system in case our calculations suck
		memoryBandwidthMB = memoryCeiling - 1000;
	}

	//Kick off thread that processes how much memory the program uses at a certain interval
	thread *memoryQueryThread = new thread(&Forest::MemoryQuery, this);

	//Kick off thread that processes how much memory the program uses at a certain interval
	thread *msyncThread = new thread(&Forest::MonitorMSYNCThreads, this);

	double threadMemoryConsumptionInMB = MemoryUtils::GetProgramMemoryConsumption();
	stringstream crappy;
	crappy << "Errant memory after processing level " << threadMemoryConsumptionInMB - MemoryUsageAtInception << " in MB!\n";
	Logger::WriteLog(crappy.str());

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

		//make this value 1 so calculations work correctly then reset
		LevelPackage levelInfo;
		levelInfo.currLevel = 1;
		levelInfo.inceptionLevelLOL = 0;
		levelInfo.threadIndex = 0;
		levelInfo.useRAM = usedRAM[0];

		bool prediction = PredictHardDiskOrRAMProcessing(levelInfo, 1);
		for(int i = 0; i < threadsToDispatch; i++)
		{
			usedRAM[i] = !prediction;
		}

		
		PListType fileReadSize = (memoryPerThread*1000000)/3.0f;
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

			if(prediction)
			{
				threadPlantSeedPoolHD = new vector<future<void>>();
			}
			else
			{
				threadPlantSeedPoolRAM = new vector<future<TreeRAM*>>();
			}

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

				if(levelRecordings.size() < levelInfo.currLevel)
				{
					levelRecordings.resize(levelInfo.currLevel);
				}
				if(mostCommonPattern.size() < levelInfo.currLevel)
				{
					mostCommonPattern.resize(levelInfo.currLevel);
					mostCommonPatternCount.resize(levelInfo.currLevel);
				}
				
				for (PListType i = 0; i < prevPListArray->size(); i++)
				{
					if((*prevPListArray)[i] != nullptr)
					{
						levelRecordings[0]++;
						if( (*prevPListArray)[i]->size() > mostCommonPatternCount[levelInfo.currLevel - 1])
						{
							mostCommonPatternCount[levelInfo.currLevel - 1] = (*prevPListArray)[i]->size();
							//cout << (*(*prevPListArray)[i])[0] << endl;
							mostCommonPattern[levelInfo.currLevel - 1] = file->fileString.substr(((*(*prevPListArray)[i])[0] - (levelInfo.currLevel)), levelInfo.currLevel);
						}
					}
				}
				
				if(coverage.size() < levelInfo.currLevel)
				{
					coverage.resize(levelInfo.currLevel);
				}
				coverage[0] = ((float)(file->fileStringSize - (256 - levelRecordings[0])))/((float)file->fileStringSize);
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
		unsigned long long memDivisor = (memoryPerThread*1000000)/3.0f;

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

		DisplayPatternsFound();
		
		//Start searching
		if(2 <= maximum)
		{
			NextLevelTreeSearch(2);
		}
			
		for(int j = 0; j < levelRecordings.size() && levelRecordings[j] != 0; j++)
		{
			
			stringstream buff;
			buff << "Level " << j + 1 << " count is " << levelRecordings[j] << " with most common pattern being: \"" << mostCommonPattern[j] << "\" occured " << mostCommonPatternCount[j] << " and coverage was " << coverage[j] << "%" << endl;
			Logger::WriteLog(buff.str());
			cout << buff.str();
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
	file->fileString.clear();
	file->fileString.reserve(0);

	processingFinished = true;
	memoryQueryThread->join();
	delete memoryQueryThread;

	processingMSYNCFinished = true;
	msyncThread->join();
	delete msyncThread;

	for(int i = 0; i < 256; i++)
	{
		delete gatedMutexes[i];
	}
	delete file;
	
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
	while(!processingMSYNCFinished)
	{
		this_thread::sleep_for(std::chrono::milliseconds(50));
		prevIndex = currIndex;
		PListArchive::syncLock.lock();
		int listCount = PListArchive::threadKillList.size();
		PListArchive::syncLock.unlock();
		//for(thread* syncThread: PListArchive::threadKillList)
		for(int i = prevIndex; i < listCount; i++)
		{
			if(PListArchive::threadKillList[i] != NULL)
			{
				PListArchive::threadKillList[i]->join();
				currIndex++;
			}
		}
		//maintenance
		//PListArchive::syncLock.lock();
		for(int i = prevIndex; i < currIndex; i++)
		{
			if(PListArchive::threadKillList[i] != NULL)
			{
				delete PListArchive::threadKillList[i];
				PListArchive::threadKillList[i] = NULL;
			}
		}
		//PListArchive::syncLock.unlock();
	}
}

void Forest::MemoryQuery()
{

	while(!processingFinished)
	{
		this_thread::sleep_for(std::chrono::milliseconds(50));
		double memoryOverflow = 0;
		overMemoryCount = MemoryUtils::IsOverMemoryCount(MemoryUsedPriorToThread, memoryBandwidthMB, memoryOverflow);
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
				
				typedef std::map<char, TreeRAM*>::iterator it_type;
				for (it_type iterator = temp->leaves.begin(); iterator != temp->leaves.end(); iterator++)
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
			
			/*stringstream builder;
			builder << "Pattern " << (*iterator).first << " occurs " << pList->size() << endl;
			Logger::WriteLog(builder.str());*/
			
			prevPListArray->push_back(pList);
		}
	}

	delete globalMap;
}

void Forest::FirstLevelHardDiskProcessing(vector<string>& backupFilenames, unsigned int z)
{
	unsigned int threadsToDispatch = numThreads - 1;
	int threadsFinished = 0;
	while(threadsFinished != threadsToDispatch)
	{
		for(int k = 0; k < threadsToDispatch; k++)
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

	if(!usingPureHD && !usingPureRAM)
	{
		usingMemoryBandwidth = false;
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
	PListType predictedMemoryForNextLevelMB = (vectorSize + pListIndexesLeft + sizeOfTreeMap)/1000000.0f;

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

	previousLevelMemoryMB = MemoryUtils::GetProgramMemoryConsumption() - MemoryUsageAtInception;

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
			for(int i = 0; i < (*prevLocalPListArray).size(); i++)
			{
				if((*prevLocalPListArray)[i] != NULL)
				{
					list<PListType> *sorting = new list<PListType>();
					copy( (*prevLocalPListArray)[i]->begin(), (*prevLocalPListArray)[i]->end(), std::back_inserter(*sorting));
					((*prevLocalPListArray)[i])->erase(((*prevLocalPListArray)[i])->begin(), ((*prevLocalPListArray)[i])->end());
					sorting->sort();
					std::copy( sorting->begin(), sorting->end(), std::back_inserter(*((*prevLocalPListArray)[i])));
					sorting->clear();
					delete sorting;
				}
			}
			
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
					threadFiles[threadNumber]->WriteArchiveMapMMAP(*(*prevLocalPListArray)[prevIndex]);
					delete (*prevLocalPListArray)[prevIndex];
				
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

bool Forest::NextLevelTreeSearch(PListType level)
{
	
	PListType threadsToDispatch = numThreads - 1;
	
	LevelPackage levelInfo;
	levelInfo.currLevel = level;
	levelInfo.inceptionLevelLOL = 0;
	levelInfo.threadIndex = 0;
	levelInfo.useRAM = usedRAM[0];

	//Do one prediction for them all
	bool prediction = PredictHardDiskOrRAMProcessing(levelInfo, levelRecordings[0]);
	
	vector<vector<string>> fileList = prevFileNameList;
	PrepDataFirstLevel(prediction, fileList, prevPListArray, globalPListArray);
	if(fileList[0].size() > 0)
	{
		
	}

	//use that one prediction
	if(usedRAM[0])
	{
		vector<vector<PListType>> balancedTruncList = ProcessThreadsWorkLoadRAM(threadsToDispatch, prevPListArray);
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
			levelInfo.currLevel = level;
			levelInfo.threadIndex = i;
			levelInfo.inceptionLevelLOL = 0;
			levelInfo.useRAM = true;
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
		for (PListType i = 0; i < localWorkingThreads.size(); i++)
		{
			LevelPackage levelInfo;
			levelInfo.currLevel = level;
			levelInfo.threadIndex = i;
			levelInfo.inceptionLevelLOL = 0;
			levelInfo.useRAM = false;
			threadsDispatched++;
			vector<PListType> temp;
			threadPool->push_back(std::async(std::launch::async, &Forest::ThreadedLevelTreeSearchRecursionList, this, prevPListArray, temp, balancedTruncList[i], levelInfo));
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

						
						/*stringstream buff;
						buff << "Thread " << localWorkingThreads[k] << " finished all processing" << endl;
						Logger::WriteLog(buff.str());
						cout << buff.str();*/
						

						//cout << "Threads in use " << threadsDispatched - threadsDefuncted << endl;

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
		
		int threadNumber = 0;
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
		
		for(int a = 0; a < threadsToDispatch; a++)
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

		for(int a = 0; a < threadsToDispatch; a++)
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

//PListType Forest::ProcessChunksAndGenerate(vector<string> fileNamesToReOpen, vector<string>& newFileNames, PListType memDivisor, unsigned int threadNum, unsigned int currLevel, bool firstLevel)
//{
//	
//	double threadMemoryConsumptionInMB = MemoryUtils::GetProgramMemoryConsumption();
//
//	int currentFile = 0;
//	int prevCurrentFile = currentFile;
//	bool memoryOverflow = false;
//	PListType interimCount = 0;
//	unsigned int threadNumber = 0;
//	unsigned int threadsToDispatch = numThreads - 1;
//	
//	vector<string> fileNamesBackup;
//	for(int a = 0; a < fileNamesToReOpen.size(); a++)
//	{
//		fileNamesBackup.push_back(fileNamesToReOpen[a]);
//	}
//
//	PListType globalTotalMemoryInBytes = 0;
//	PListType globalTotalLeafSizeInBytes = 0;
//
//	vector<PListArchive*> patternFiles;
//	PListType internalRemovedCount = 0;
//
//	while(currentFile < fileNamesBackup.size())
//	{
//		memoryOverflow = false;
//
//		vector<PListArchive*> archiveCollection;
//		map<PatternType, vector<PListType>*> finalMetaDataMap;
//
//		globalTotalLeafSizeInBytes = 0;
//		globalTotalMemoryInBytes = 32;
//			
//		for(int a = 0; a < fileNamesBackup.size() - prevCurrentFile; a++)
//		{
//
//			archiveCollection.push_back(new PListArchive(fileNamesBackup[a+prevCurrentFile]));
//
//			//If file size is 0 or does not exist, we skip
//			if(archiveCollection[a]->IsEndOfFile())
//			{
//				if(!memoryOverflow)
//				{
//					currentFile++;
//				}
//				archiveCollection[a]->CloseArchiveMMAP();
//				delete archiveCollection[a];
//				continue;
//			}
//			
//			//Our job is to trust whoever made the previous chunk made it within the acceptable margin of error so we compensate by saying take up to double the size if the previous
//			//chunk went a little over the allocation constraint
//			
//			vector<vector<PListType>*> packedPListArray;
//			vector<string> *stringBuffer = NULL;
//			PListArchive *stringBufferFile = NULL;
//			string fileNameForLater ="";
//			PListType packedPListSize = 0; 
//			string fileName ="";
//			bool foundAHit = true;
//
//			if(finalMetaDataMap.size() > 0)
//			{
//
//				globalTotalLeafSizeInBytes = (finalMetaDataMap.size() + 1)*32;
//				globalTotalLeafSizeInBytes += (finalMetaDataMap.size() + 1)*(finalMetaDataMap.begin()->first.capacity() + sizeof(string));
//				globalTotalLeafSizeInBytes += finalMetaDataMap.size() * 32;
//			}
//
//			if((globalTotalLeafSizeInBytes/1000000.0f) + (globalTotalMemoryInBytes/1000000.0f) > 2.0f*memDivisor/1000000.0f)
//			{
//				stringstream crap;
//				crap << "Actual overflow " << MemoryUtils::GetProgramMemoryConsumption() - threadMemoryConsumptionInMB << " in MB!\n";
//				crap << "Quick approximation at Process Chunks And Generate of " << (globalTotalLeafSizeInBytes/1000000.0f) + (globalTotalMemoryInBytes/1000000.0f) << " in MB!\n";
//				Logger::WriteLog(crap.str());
//				memoryOverflow = true;
//			}
//
//			if(!memoryOverflow)
//			{
//				archiveCollection[a]->GetPListArchiveMMAP(packedPListArray);
//				packedPListSize = packedPListArray.size();
//
//				std::string::size_type i = archiveCollection[a]->fileName.find(".txt");
//				string tempString = archiveCollection[a]->fileName;
//				tempString.erase(i, 8);
//				tempString.erase(0, 7);
//				
//				fileNameForLater.append(tempString);
//				fileName = fileNameForLater;
//				fileName.append("Patterns");
//				stringBufferFile = new PListArchive(fileName);
//				std::string::size_type l = archiveCollection[a]->fileName.find("_");
//				string copyString = archiveCollection[a]->fileName;
//				copyString.erase(0, l + 1);
//				std::string::size_type k = copyString.find(".txt");
//				copyString.erase(k, 4);
//				std::string::size_type sz;   // alias of size_t
//				PListType sizeOfPackedPList = std::stoll (copyString,&sz);
//				stringBuffer = stringBufferFile->GetPatterns(currLevel, packedPListSize);
//				
//			}
//			else
//			{
//				
//				std::string::size_type i = archiveCollection[a]->fileName.find(".txt");
//				string tempString = archiveCollection[a]->fileName;
//				tempString.erase(i, 8);
//				tempString.erase(0, 7);
//
//				fileNameForLater.append(tempString);
//				fileName = fileNameForLater;
//				fileName.append("Patterns");
//				stringBufferFile = new PListArchive(fileName);
//				std::string::size_type j = archiveCollection[a]->fileName.find("_");
//				string copyString = archiveCollection[a]->fileName;
//				copyString.erase(0, j + 1);
//				std::string::size_type k = copyString.find(".txt");
//				copyString.erase(k, 4);
//				std::string::size_type sz;   // alias of size_t
//				PListType sizeOfPackedPList = std::stoll (copyString,&sz);
//				stringBuffer = stringBufferFile->GetPatterns(currLevel, sizeOfPackedPList);
//				packedPListSize = sizeOfPackedPList;
//				
//				foundAHit = false;
//				
//				if(sizeOfPackedPList > 0)
//				{
//					for(PListType z = 0; z < stringBuffer->size(); z++)
//					{
//						if(finalMetaDataMap.find((*stringBuffer)[z]) != finalMetaDataMap.end())
//						{
//							foundAHit = true;
//							break;
//						}
//					}
//					if(foundAHit)
//					{
//						archiveCollection[a]->GetPListArchiveMMAP(packedPListArray); //Needs MB
//					}
//				}
//			}
//
//			PListType countAdded = 0;
//			if(foundAHit)
//			{
//  				for(PListType partialLists = 0; partialLists < packedPListArray.size(); partialLists++)
//				{
//					try
//					{
//						
//						string pattern = (*stringBuffer)[partialLists];
//							
//						//This allows us to back fill the others iterations when this didn't have a pattern
//						if(finalMetaDataMap.find(pattern) == finalMetaDataMap.end())
//						{
//							if(!memoryOverflow)
//							{
//								finalMetaDataMap[pattern] = new vector<PListType>(packedPListArray[partialLists]->begin(), packedPListArray[partialLists]->end());
//								delete packedPListArray[partialLists];
//								packedPListArray[partialLists] = NULL;
//								countAdded++;
//								//Only add size because it is a new vector and add in capacity
//								globalTotalMemoryInBytes += sizeof(PListType)*finalMetaDataMap[pattern]->capacity();
//							}
//						}
//						else
//						{
//							//First subract original vector size in capacity
//							globalTotalMemoryInBytes -= sizeof(PListType)*finalMetaDataMap[pattern]->capacity();
//							finalMetaDataMap[pattern]->insert(finalMetaDataMap[pattern]->end(), packedPListArray[partialLists]->begin(), packedPListArray[partialLists]->end());
//							delete packedPListArray[partialLists];
//							packedPListArray[partialLists] = NULL;
//							countAdded++;
//							//then add new size in capacity
//							globalTotalMemoryInBytes += sizeof(PListType)*finalMetaDataMap[pattern]->capacity();
//						}
//						
//					}
//					catch(exception e)
//					{
//						cout << "System exception: " << e.what() << endl;
//					}
//				}
//			}
//			
//				
//			if(foundAHit)
//			{
//				/*if(countAdded != packedPListSize)
//				{*/
//
//					archiveCollection[a]->CloseArchiveMMAP();
//					stringBufferFile->CloseArchiveMMAP();
//
//					DeleteChunk(fileNameForLater, ARCHIVE_FOLDER);
//					
//					delete archiveCollection[a];
//					delete stringBufferFile;
//
//					PListType newCount = packedPListSize - countAdded;
//					if(newCount > 0)
//					{
//						string backupFile = fileNameForLater;
//						std::string::size_type j = backupFile.find("_");
//						backupFile.erase(j + 1, backupFile.size() - j);
//						stringstream buffy;
//						buffy << backupFile << newCount;
//						fileNamesBackup[prevCurrentFile + a] = buffy.str();
//					
//						PListType testCount = 0;
//						PListArchive* archiveCollective = new PListArchive(fileNamesBackup[prevCurrentFile + a], true);
//						for(PListType partialLists = 0; partialLists < packedPListSize; partialLists++)
//						{
//							if(packedPListArray[partialLists] != NULL)
//							{
//								archiveCollective->WriteArchiveMapMMAP(*packedPListArray[partialLists], (*stringBuffer)[partialLists]);
//								delete packedPListArray[partialLists];
//								packedPListArray[partialLists] = NULL;
//								testCount++;
//							}
//						}
//
//						//cout << "file name: " << archiveCollective->fileName << " size of new plist buffer: " << pListNewBufferCount << " size of new string buffer: " << archiveCollective->stringBuffer.size() << endl;
//
//						archiveCollective->DumpPatternsToDisk(currLevel);
//						archiveCollective->WriteArchiveMapMMAP(vector<PListType>(), "", true);
//						archiveCollective->CloseArchiveMMAP();
//
//						delete archiveCollective;
//					}
//				/*}
//				else
//				{
//					archiveCollection[a]->DumpContents();
//					archiveCollection[a]->CloseArchiveMMAP();
//
//					delete archiveCollection[a];
//
//					stringBufferFile->DumpContents();
//					stringBufferFile->CloseArchiveMMAP();
//
//					delete stringBufferFile;
//				}*/
//
//				//delete packedPListArray;
//				if(stringBuffer != NULL)
//				{
//					stringBuffer->clear();
//					delete stringBuffer;
//				}
//			}
//			else
//			{
//			
//				archiveCollection[a]->CloseArchiveMMAP();
//
//				delete archiveCollection[a];
//
//				stringBufferFile->CloseArchiveMMAP();
//
//				delete stringBufferFile;
//				
//				stringBuffer->clear();
//				delete stringBuffer;
//			}
//			if(!memoryOverflow || countAdded == packedPListSize || packedPListSize == 0)
//			{
//				currentFile++;
//			}
//		}
//
//		//thread files
//		PListArchive* currChunkFile = NULL;
//		bool notBegun = true;
//		PListType removedPatterns = 0;
//
//		for(it_map_list_p_type iterator = finalMetaDataMap.begin(); iterator != finalMetaDataMap.end(); iterator++)
//		{
//			if(notBegun)
//			{
//				notBegun = false;
//				if(currChunkFile != NULL)
//				{
//					//currChunkFile->WriteArchiveUpFront(PListsToWrite);
//					currChunkFile->WriteArchiveMapMMAP(vector<PListType>(), "", true);
//					currChunkFile->CloseArchiveMMAP();
//					delete currChunkFile;
//				}
//				
//				if(firstLevel)
//				{
//					stringstream fileNameage;
//					stringstream fileNameForPrevList;
//					fileIDMutex->lock();
//					fileID++;
//					fileNameage << ARCHIVE_FOLDER << "PListChunks" << fileID << ".txt";
//					fileNameForPrevList << "PListChunks" << fileID;
//					fileIDMutex->unlock();
//					 
//					prevFileNameList[threadNumber].push_back(fileNameForPrevList.str());
//				
//					currChunkFile = new PListArchive(fileNameForPrevList.str(), true);
//	
//					threadNumber++;
//					threadNumber %= threadsToDispatch;
//				}
//				else
//				{
//					
//					stringstream fileNameForPrevList;
//					fileIDMutex->lock();
//					fileID++;
//					fileNameForPrevList << "PListChunks" << fileID;
//					fileIDMutex->unlock();
//
//					newFileNames.push_back(fileNameForPrevList.str());
//				
//					currChunkFile = new PListArchive(fileNameForPrevList.str(), true);
//				}
//			}
//			
//			if(iterator->second->size() >= minOccurrence /*|| (outlierScans && iterator->second->size() == 1)*/)
//			{
//				currChunkFile->WriteArchiveMapMMAP(*iterator->second);
//				interimCount++;
//
//				if(mostCommonPattern.size() < currLevel)
//				{
//					mostCommonPattern.resize(currLevel);
//					mostCommonPatternCount.resize(currLevel);
//				}
//				
//				if(iterator->second->size() > mostCommonPatternCount[currLevel - 1])
//				{
//					mostCommonPatternCount[currLevel - 1] = iterator->second->size();
//				
//					mostCommonPattern[currLevel - 1] = iterator->first;
//				}
//			}
//			else
//			{
//				removedPatterns++;
//			}
//			
//			delete iterator->second;
//		}
//
//		if(currChunkFile != NULL)
//		{
//			currChunkFile->WriteArchiveMapMMAP(vector<PListType>(), "", true);
//			if(currChunkFile->mappingIndex != 0)
//			{
//				currChunkFile->CloseArchiveMMAP();
//				delete currChunkFile;
//			}
//			else
//			{
//				currChunkFile->CloseArchiveMMAP();
//				delete currChunkFile;
//				DeleteChunk(newFileNames[newFileNames.size() - 1], ARCHIVE_FOLDER);
//				newFileNames.pop_back();
//			}
//		}
//		
//		countMutex->lock();
//		eradicatedPatterns += removedPatterns;
//		countMutex->unlock();
//
//		for(int a = prevCurrentFile; a < currentFile; a++)
//		{
//			DeleteChunk(fileNamesBackup[a], ARCHIVE_FOLDER);
//		}
//		prevCurrentFile = currentFile;
//	}
//
//	countMutex->lock();
//	if(levelRecordings.size() < currLevel)
//	{
//		levelRecordings.resize(currLevel);
//	}
//	levelRecordings[currLevel - 1] += interimCount;
//
//	if(coverage.size() < currLevel)
//	{
//		coverage.resize(currLevel);
//	}
//	coverage[currLevel - 1] += ((float)(interimCount))/((float)file->fileStringSize);
//	
//	stringstream buffy;
//	buffy << currLevel << " with a total of " << levelRecordings[currLevel - 1] << " using HD" << endl;
//	Logger::WriteLog(buffy.str());
//
//	if(currLevel > currentLevelVector[threadNum])
//	{
//		currentLevelVector[threadNum] = currLevel;
//	}
//	countMutex->unlock();
//
//	return interimCount;
//}

PListType Forest::ProcessChunksAndGenerate(vector<string> fileNamesToReOpen, vector<string>& newFileNames, PListType memDivisor, unsigned int threadNum, unsigned int currLevel, bool firstLevel)
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

			if((globalTotalLeafSizeInBytes/1000000.0f) + (globalTotalMemoryInBytes/1000000.0f) > 2.0f*memDivisor/1000000.0f/* || overMemoryCount*/)
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
				for(int earlyWriteIndex = a; earlyWriteIndex < archiveCollection.size(); earlyWriteIndex++)
				{
				
					std::string::size_type i = archiveCollection[earlyWriteIndex]->fileName.find(".txt");
					string tempString = archiveCollection[earlyWriteIndex]->fileName;
					tempString.erase(i, 8);
					tempString.erase(0, 7);

					fileName = tempString;
					fileName.append("Patterns");
					stringBufferFile = new PListArchive(fileName);
					std::string::size_type j = archiveCollection[earlyWriteIndex]->fileName.find("_");
					string copyString = archiveCollection[earlyWriteIndex]->fileName;
					copyString.erase(0, j + 1);
					std::string::size_type k = copyString.find(".txt");
					copyString.erase(k, 4);
					std::string::size_type sz;   // alias of size_t
					PListType sizeOfPackedPList = std::stoll (copyString,&sz);
					stringBuffer = stringBufferFile->GetPatterns(currLevel, sizeOfPackedPList);
					packedPListSize = sizeOfPackedPList;

					stringBufferFile->CloseArchiveMMAP();
					delete stringBufferFile;
				
					totalPatterns += packedPListSize;

					foundAHit = false;
				
					if(sizeOfPackedPList > 0)
					{
						//for(string_it_type iterator = stringBuffer->begin(); iterator != stringBuffer->end(); iterator++)
						for(PListType z = 0; z < stringBuffer->size(); z++)
						{
							if(finalMetaDataMap.find((*stringBuffer)[z]) != finalMetaDataMap.end())
							{
								patternsThatCantBeDumped.push_back((*stringBuffer)[z]);
							}
						}
					}
					stringBuffer->clear();
					delete stringBuffer;
				}
				
				stringstream sizeDifference;
				sizeDifference << "Actual patterns: " << totalPatterns << " and total patterns we can get rid of: " << totalPatterns - patternsThatCantBeDumped.size() << endl;
				if(totalPatterns != totalPatterns - patternsThatCantBeDumped.size())
				{
					sizeDifference << "We don't have a match!" << endl;
				}
				Logger::WriteLog(sizeDifference.str());
				patternsThatCantBeDumped.unique();




				//ADDED CODE

				//thread files
				PListArchive* currChunkFile = NULL;
				bool notBegun = true;
				PListType removedPatterns = 0;

				it_map_list_p_type iterator = finalMetaDataMap.begin();
				while( iterator != finalMetaDataMap.end())
				{
					string_it_type it = find (patternsThatCantBeDumped.begin(), patternsThatCantBeDumped.end(), iterator->first);
					if(it == patternsThatCantBeDumped.end())
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
					else
					{
						iterator++;
					}
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


				//END OF ADDED CODE



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
				stringBuffer = stringBufferFile->GetPatterns(currLevel, sizeOfPackedPList);
				packedPListSize = sizeOfPackedPList;
				
				foundAHit = false;
				
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
					if(foundAHit)
					{
						archiveCollection[a]->GetPListArchiveMMAP(packedPListArray); //Needs MB
					}
				}

			}

			PListType countAdded = 0;
			if(foundAHit)
			{
  				for(PListType partialLists = 0; partialLists < packedPListArray.size(); partialLists++)
				{
					try
					{
						
						string pattern = (*stringBuffer)[partialLists];
							
						//This allows us to back fill the others iterations when this didn't have a pattern
						if(finalMetaDataMap.find(pattern) == finalMetaDataMap.end())
						{
							if(!memoryOverflow)
							{
								finalMetaDataMap[pattern] = new vector<PListType>(packedPListArray[partialLists]->begin(), packedPListArray[partialLists]->end());
								delete packedPListArray[partialLists];
								packedPListArray[partialLists] = NULL;
								countAdded++;
								//Only add size because it is a new vector and add in capacity
								globalTotalMemoryInBytes += sizeof(PListType)*finalMetaDataMap[pattern]->capacity();
							}
						}
						else
						{
							//First subract original vector size in capacity
							globalTotalMemoryInBytes -= sizeof(PListType)*finalMetaDataMap[pattern]->capacity();
							finalMetaDataMap[pattern]->insert(finalMetaDataMap[pattern]->end(), packedPListArray[partialLists]->begin(), packedPListArray[partialLists]->end());
							delete packedPListArray[partialLists];
							packedPListArray[partialLists] = NULL;
							countAdded++;
							//then add new size in capacity
							globalTotalMemoryInBytes += sizeof(PListType)*finalMetaDataMap[pattern]->capacity();
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
				//delete packedPListArray;
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
					//currChunkFile->WriteArchiveUpFront(PListsToWrite);
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
						
						currChunkFiles[pattern]->WriteArchiveMapMMAP(*(packedPListArray[partialLists])/*, "", true*/);
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
	int currLevel = levelInfo.currLevel;
	PListType newPatternCount = 0;
	//Divide between file load and previous level pLists and leave some for new lists haha 
	unsigned long long memDivisor = (memoryPerThread*1000000)/3.0f;
		
	bool morePatternsToFind = false;

	unsigned long long fileIters = file->fileStringSize/memDivisor;
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
			int zed = 0;
			while(!archive.IsEndOfFile())
			{
				if(archive.patternName.compare("PListChunks41") == 0)
				{
					cout << "Shitlizard!" << endl;
				}
				vector<vector<PListType>*> packedPListArray;
				archive.GetPListArchiveMMAP(packedPListArray, memDivisor/1000000.0f); 
				
				zed++;

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

					int firstIndex = minimum/memDivisor;
					int lastIndex = fileIters;
					/*int lastIndex = maximum/memDivisor;
					if(lastIndex == fileIters - 1 && maximum%memDivisor != 0)
					{
						lastIndex = fileIters;
					}*/

					int threadChunkToUse = threadNum;
					//for(int j = 0; j < fileIters; j++)
					for(int j = firstIndex; j < lastIndex && minimum != -1; j++)
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
								//Shared file space test
								fileChunks.push_back("");
								fileChunks[fileChunks.size() - 1].resize(patternCount);
							
								PListType offset = memDivisor*j;
								FileReader fileReaderTemp(file->fileName);
								fileReaderTemp.copyBuffer->seekg( offset );
								fileReaderTemp.copyBuffer->read( &fileChunks[fileChunks.size() - 1][0], patternCount );
							
								//chunkOrigin[threadNum] = j;

								threadChunkToUse = fileChunks.size() - 1;

								chunkIndexToFileChunk[j] = fileChunks.size() - 1;
							
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
									//if((globalTotalLeafSizeInBytes + globalTotalMemoryInBytes < memDivisor) /*&& !overMemoryCount*/)
									//{
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
															if(leaf.leaves.find(pattern) != leaf.leaves.end())
															{
																globalTotalMemoryInBytes -= leaf.leaves[pattern].pList.capacity()*sizeof(PListType);
															}
															leaf.addLeaf((*pList)[k]+1, pattern);
															globalTotalMemoryInBytes += leaf.leaves[pattern].pList.capacity()*sizeof(PListType);
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
									//}
									//else
									//{
								

									//	//stringstream crap;
									//	//crap << "Approximation overflow at Process HD of " << sizeInMB << " in MB!\n";
									//	//crap << "Overflow at Process HD of " << currMemoryOverflow << " in MB!\n";
									//	//crap << "Quick approximation at Process HD of " << (globalTotalLeafSizeInBytes/1000000.0f) + (globalTotalMemoryInBytes/1000000.0f) << " in MB!\n";
									//	//Logger::WriteLog(crap.str());

									//	globalTotalLeafSizeInBytes = 0;
									//	globalTotalMemoryInBytes = 32;
									//	//if true already do not write again until memory is back in our hands
									//	if(!justPassedMemorySize && leaf.leaves.size() > 0)
									//	{

									//		PListType patterns = leaf.leaves.size();
								
									//		justPassedMemorySize = true;
									//		stringstream stringBuilder;
									//		fileIDMutex->lock();
									//		fileID++;
									//		stringBuilder << fileID;
									//		fileIDMutex->unlock();
									//		fileNamesToReOpen.push_back(CreateChunkFile(stringBuilder.str(), leaf, threadNum, currLevel));
								
									//	}
									//	else
									//	{
								
									//		//If memory is unavailable sleep for one second
									//		//std::this_thread::sleep_for (std::chrono::seconds(1));
									//	}
									//	//i--;
									//}
								}
								prevLeafIndex[i] = k;
								justPassedMemorySize = false;
							}
							else
							{
								

								//stringstream crap;
								//crap << "Approximation overflow at Process HD of " << sizeInMB << " in MB!\n";
								//crap << "Overflow at Process HD of " << currMemoryOverflow << " in MB!\n";
								//crap << "Quick approximation at Process HD of " << (globalTotalLeafSizeInBytes/1000000.0f) + (globalTotalMemoryInBytes/1000000.0f) << " in MB!\n";
								//Logger::WriteLog(crap.str());

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
									fileNamesToReOpen.push_back(CreateChunkFile(stringBuilder.str(), leaf, threadNum, currLevel));
								
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
							fileNamesToReOpen.push_back(CreateChunkFile(stringBuilder.str(), leaf, threadNum, currLevel));

						}
					}
				
				
		
					for(PListType pTits = 0; pTits < packedPListArray.size(); pTits++)
					{
						delete packedPListArray[pTits];
					}
				}
				//delete packedPListArray;

			}
			archive.CloseArchiveMMAP();

		}

		if(levelToOutput == 0 || (levelToOutput != 0 && currLevel >= levelToOutput))
		{
			newPatternCount += ProcessChunksAndGenerate(fileNamesToReOpen, newFileNames, memDivisor, threadNum, currLevel);
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
		DeleteArchives(fileList, ARCHIVE_FOLDER);
	}
	return morePatternsToFind;
}

bool Forest::DispatchNewThreadsRAM(PListType newPatternCount, bool& morePatternsToFind, vector<vector<PListType>*>* prevLocalPListArray, LevelPackage levelInfo, bool& isThreadDefuncted)
{
	bool dispatchedNewThreads = false;
	bool alreadyUnlocked = false;
	countMutex->lock();

	int threadsToDispatch = numThreads - 1;
	int unusedCores = (threadsToDispatch - (threadsDispatched - threadsDefuncted)) + 1;
	if(prevLocalPListArray->size() < unusedCores && unusedCores > 1)
	{
		unusedCores = prevLocalPListArray->size();
	}
	//Need to have an available core, need to still have patterns to search and need to have more than 1 pattern to be worth splitting up the work
	if(unusedCores > 1 && morePatternsToFind && prevLocalPListArray->size() > 1)
	{
		//OLD WAY THREAD PRORITY//
		/*unsigned int levelCount = 1000000000;
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
			if(threadPriority[z] == levelInfo.threadIndex && currentLevelVector[threadPriority[z]] == levelCount)
			{
				spawnThreads = true;
			}
		}*/
		//END OF OLD WAY THREAD PRORITY//
		bool spawnThreads = true;
		//If this thread is at the lowest level of progress spawn new threads
		if(spawnThreads)
		{
			
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
							tally += (*prevLocalPListArray)[balancedTruncList[z][d]]->size();
						}
					}

					dispatchedNewThreads = true;
					//cout << "Thread " << levelInfo.threadIndex << " has priority and is at level " << levelInfo.currLevel << endl;

					LevelPackage levelInfoRecursion;
					levelInfoRecursion.currLevel = levelInfo.currLevel;
					levelInfoRecursion.threadIndex = levelInfo.threadIndex;
					levelInfoRecursion.inceptionLevelLOL = levelInfo.inceptionLevelLOL + 1;
					levelInfoRecursion.useRAM = true;

					//cout << "Current threads in use: " << threadsDispatched - threadsDefuncted + localWorkingThreads.size() - 1 << endl;

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
					WaitForThreads(localWorkingThreads, localThreadPool, true);

					localThreadPool->erase(localThreadPool->begin(), localThreadPool->end());
					(*localThreadPool).clear();
					delete localThreadPool;
					morePatternsToFind = false;
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
		unusedCores = newPatternCount;
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

					vector<future<void>> *localThreadPool = new vector<future<void>>();
					for (PListType i = 0; i < localWorkingThreads.size(); i++)
					{
						threadsDispatched++;
						vector<PListType> temp;
						localThreadPool->push_back(std::async(std::launch::async, &Forest::ThreadedLevelTreeSearchRecursionList, this, prevPListArray, temp, balancedTruncList[i], levelInfoRecursion));
					}
					countMutex->unlock();
						
					alreadyUnlocked = true;
					WaitForThreads(localWorkingThreads, localThreadPool, true);

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
	bool continueSearching = true;
	int threadsToDispatch = numThreads - 1;
	int totalTallyRemovedPatterns = 0;
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
				//unsigned char value = file->fileString[(*pList)[k]];
				string value = file->fileString.substr((*pList)[k], 1);
				if(patternToSearchFor.size() == 0 || value[0] == patternToSearchFor[levelInfo.currLevel - 1])
				{
					leaf.addLeaf(value[0], (*pList)[k] + 1);
				}
			}
		}
		
			
		PListType removedPatternsTemp = 0;
		vector<vector<PListType>*>* newList = leaf.GetLeafPLists(removedPatternsTemp, minOccurrence);
		totalTallyRemovedPatterns += removedPatternsTemp;
		

		if(newList != NULL)
		{
			globalLocalPListArray->insert(globalLocalPListArray->end(), newList->begin(), newList->end());
			newList->erase(newList->begin(), newList->end());
			delete newList;
		}

		delete leaf.GetPList();

		delete (*prevLocalPListArray)[i];
	}
	
	countMutex->lock();
	eradicatedPatterns += totalTallyRemovedPatterns;
	countMutex->unlock();

	countMutex->lock();

	if(mostCommonPattern.size() < levelInfo.currLevel)
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
	}
		

	if(levelRecordings.size() < levelInfo.currLevel)
	{
		levelRecordings.resize(levelInfo.currLevel);
	}
	levelRecordings[levelInfo.currLevel - 1] += globalLocalPListArray->size();

	if(coverage.size() < levelInfo.currLevel)
	{
		coverage.resize(levelInfo.currLevel);
	}
	coverage[levelInfo.currLevel - 1] += ((float)(globalLocalPListArray->size()))/((float)file->fileStringSize);

	stringstream buffy;
	buffy << levelInfo.currLevel << " with a total of " << levelRecordings[levelInfo.currLevel - 1] << " using RAM" << endl;
	Logger::WriteLog(buffy.str());
		
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

//TreeHD* Forest::PlantTreeSeedThreadHD(PListType positionInFile, PListType startPatternIndex, PListType numPatternsToSearch, unsigned int threadNum)
//{
//
//	double threadMemoryConsumptionInMB = MemoryUtils::GetProgramMemoryConsumption();
//	stringstream crappy;
//	crappy << "Errant memory after processing level " << threadMemoryConsumptionInMB - MemoryUsageAtInception << " in MB!\n";
//	Logger::WriteLog(crappy.str());
//
//	PListType memDivisorInMB = (memoryPerThread/3.0f);
//	//used primarily for just storage containment
//	TreeHD leaf;
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
//		leaf.addLeaf(i+positionInFile+1,file->fileString.substr(i, 1));
//		
//		if(overMemoryCount)
//		{
//			threadMemoryConsumptionInMB = MemoryUtils::GetProgramMemoryConsumption();
//			crappy.str("");;
//			crappy << "Errant memory after processing level " << threadMemoryConsumptionInMB - MemoryUsageAtInception << " in MB!\n";
//			Logger::WriteLog(crappy.str());
//
//
//			stringstream stringBuilder;
//			fileIDMutex->lock();
//			fileID++;
//			stringBuilder << fileID;
//			fileIDMutex->unlock();
//
//
//			countMutex->lock();
//			newFileNameList[threadNum].push_back(CreateChunkFile(stringBuilder.str(), leaf, threadNum, globalLevel));
//			countMutex->unlock();
//
//			threadMemoryConsumptionInMB = MemoryUtils::GetProgramMemoryConsumption();
//			crappy.str("");;
//			crappy << "Errant memory after processing level " << threadMemoryConsumptionInMB - MemoryUsageAtInception << " in MB!\n";
//			Logger::WriteLog(crappy.str());
//
//		}
//#endif
//	}
//
//	
//	if(leaf.leaves.size() > 0)
//	{
//
//		threadMemoryConsumptionInMB = MemoryUtils::GetProgramMemoryConsumption();
//		crappy.str("");;
//		crappy << "Errant memory after processing level " << threadMemoryConsumptionInMB - MemoryUsageAtInception << " in MB!\n";
//		Logger::WriteLog(crappy.str());
//
//		stringstream stringBuilder;
//		fileIDMutex->lock();
//		fileID++;
//		stringBuilder << fileID;
//		fileIDMutex->unlock();
//
//		countMutex->lock();
//		newFileNameList[threadNum].push_back(CreateChunkFile(stringBuilder.str(), leaf, threadNum, globalLevel));
//		countMutex->unlock();
//
//		threadMemoryConsumptionInMB = MemoryUtils::GetProgramMemoryConsumption();
//		crappy.str("");;
//		crappy << "Errant memory after processing level " << threadMemoryConsumptionInMB - MemoryUsageAtInception << " in MB!\n";
//		Logger::WriteLog(crappy.str());
//	}
//	
//	
//	delete leaf.GetPList();
//
//
//	threadMemoryConsumptionInMB = MemoryUtils::GetProgramMemoryConsumption();
//	crappy.str("");
//	crappy << "Errant memory after processing level " << threadMemoryConsumptionInMB - MemoryUsageAtInception << " in MB!\n";
//	Logger::WriteLog(crappy.str());
//
//
//	return NULL;
//}

void Forest::PlantTreeSeedThreadHD(PListType positionInFile, PListType startPatternIndex, PListType numPatternsToSearch, unsigned int threadNum)
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
			newFileNameList[threadNum].push_back(CreateChunkFile(stringBuilder.str(), leaf, threadNum, globalLevel));
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
		newFileNameList[threadNum].push_back(CreateChunkFile(stringBuilder.str(), leaf, threadNum, globalLevel));

	}

	return;
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
		string value = file->fileString.substr(i, 1);
		if(patternToSearchFor.size() == 0 || value[0] == patternToSearchFor[0])
		{
			leaf->addLeaf(file->fileString[i], i+positionInFile+1);
		}
#endif
	}


	vector<unsigned char> listToDos;

	typedef std::map<char, TreeRAM*>::iterator it_type;
	for(it_type iterator = leaf->leaves.begin(); iterator != leaf->leaves.end(); iterator++)
	{
		
		vector<PListType>* pList = iterator->second->GetPList();
		unsigned char index = ((unsigned char)iterator->first);
		if((*prevPListArray)[index] == NULL)
		{
			if(gatedMutexes[index]->try_lock())
			{
				/*list<PListType> *sorting = new list<PListType>();
				copy( pList->begin(), pList->end(), std::back_inserter(*sorting));
				pList->erase(pList->begin(), pList->end());
				sorting->sort();
				std::copy( sorting->begin(), sorting->end(), std::back_inserter(*pList));
				sorting->clear();
				delete sorting;*/

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
				
				/*list<PListType> *sorting = new list<PListType>();
				copy( (*prevPListArray)[index]->begin(), (*prevPListArray)[index]->end(), std::back_inserter(*sorting));
				(*prevPListArray)[index]->erase((*prevPListArray)[index]->begin(), (*prevPListArray)[index]->end());
				sorting->sort();
				std::copy( sorting->begin(), sorting->end(), std::back_inserter(*(*prevPListArray)[index]));
				sorting->clear();
				delete sorting;*/

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
		
			vector<PListType>* pList = leaf->leaves.find(listToDos[i])->second->GetPList();
			if((*prevPListArray)[listToDos[i]] == NULL)
			{
				if(gatedMutexes[listToDos[i]]->try_lock())
				{
					/*list<PListType> *sorting = new list<PListType>();
					copy( pList->begin(), pList->end(), std::back_inserter(*sorting));
					pList->erase(pList->begin(), pList->end());
					sorting->sort();
					std::copy( sorting->begin(), sorting->end(), std::back_inserter(*pList));
					sorting->clear();
					delete sorting;*/

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

					/*list<PListType> *sorting = new list<PListType>();
					copy( (*prevPListArray)[listToDos[i]]->begin(), (*prevPListArray)[listToDos[i]]->end(), std::back_inserter(*sorting));
					(*prevPListArray)[listToDos[i]]->erase((*prevPListArray)[listToDos[i]]->begin(), (*prevPListArray)[listToDos[i]]->end());
					sorting->sort();
					std::copy( sorting->begin(), sorting->end(), std::back_inserter(*(*prevPListArray)[listToDos[i]]));
					sorting->clear();
					delete sorting;*/

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

	for(it_type iterator = leaf->leaves.begin(); iterator != leaf->leaves.end(); iterator++)
	{
		delete iterator->second;
	}

	delete leaf->GetPList();
	delete leaf;
	return NULL;
}

string Forest::CreateChunkFile(string fileName, TreeHD& leaf, unsigned int threadNum, PListType currLevel)
{
	string fileNameToReOpen;
	
	stringstream archiveName;
	string archiveFileType = "PListChunks";
	
	archiveName << archiveFileType << fileName << "_" << leaf.leaves.size();
	
	PListArchive* archiveCollective = new PListArchive(archiveName.str(), true);
	fileNameToReOpen = archiveName.str();
	typedef std::map<string, TreeHD>::iterator it_type;

	/*stringstream patternsToWrite;
	patternsToWrite << "Patterns to write to disk: " << leaf.leaves.size() << endl;
	Logger::WriteLog(patternsToWrite.str());*/
	it_type iterator = leaf.leaves.begin();
	while(iterator != leaf.leaves.end()) 
	{
		archiveCollective->WriteArchiveMapMMAP(iterator->second.pList, iterator->first, false);
		/*if(overMemoryCount)
		{
			archiveCollective->WriteArchiveMapMMAP(vector<PListType>(), "", true);
		}*/
		iterator = leaf.leaves.erase(iterator);
	}
	map<string, TreeHD> test;
	test.swap(leaf.leaves);
	leaf.pList.clear();

	archiveCollective->DumpPatternsToDisk(currLevel);
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
			stringstream builder;
			builder << "Archives succesfully deleted " << fileNameToBeRemoved << '\n';
			Logger::WriteLog(builder.str());
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
		stringstream builder;
		builder << "Archive succesfully deleted " << fileNameToBeRemoved << '\n';
		Logger::WriteLog(builder.str());
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
		stringstream builder;
		builder << "Chunk succesfully deleted " << fileNameToBeRemoved << '\n';
		Logger::WriteLog(builder.str());
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
		stringstream builder;
		builder << "Chunk succesfully deleted " << fileNameToBeRemovedPatterns << '\n';
		Logger::WriteLog(builder.str());
	}
	
}