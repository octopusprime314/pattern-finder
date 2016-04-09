#include "Forest.h"
#include "MemoryUtils.h"
#include <locale>
#include <list>
#if defined(_WIN64) || defined(_WIN32)
	#include <direct.h>
#elif defined(__linux__)
	#include "sys/stat.h"
#endif


Forest::Forest(int argc, char **argv)
{
	//system("rm -r ../Log/*");
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

	CommandLineParser(argc, argv);

	MemoryUsageAtInception = MemoryUtils::GetProgramMemoryConsumption();
	overMemoryCount = MemoryUsageAtInception;
	processingFinished = false;
	
	countMutex = new mutex();
	threadPool = new vector<future<void>>();
	threadPlantSeedPoolHD = new vector<future<TreeHD*>>();
	threadPlantSeedPoolRAM = new vector<future<TreeRAM*>>();
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
	
	//Assume start with RAM
	usedRAM.resize(threadsToDispatch);
	for(int i = 0; i < threadsToDispatch; i++)
	{
		usedRAM[i] = true;
	}
	
	//If memory bandwidth not an input
	if(!usingMemoryBandwidth)
	{
		//Leave 1 GB to spare for operating system in case our calculations suck
		memoryBandwidthMB = MemoryUtils::GetAvailableRAMMB() - 1000;
	}

	//Kick off thread that processes how much memory the program uses at a certain interval
	thread *memoryQueryThread = new thread(&Forest::MemoryQuery, this);

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
				
				levelRecordings[0] = prevPListArray->size();

				for (PListType i = 0; i < prevPListArray->size(); i++)
				{
					if((*prevPListArray)[i] != nullptr && (*prevPListArray)[i]->size() > mostCommonPatternCount[levelInfo.currLevel - 1])
					{
						mostCommonPatternCount[levelInfo.currLevel - 1] = (*prevPListArray)[i]->size();
						cout << (*(*prevPListArray)[i])[0] << endl;
						mostCommonPattern[levelInfo.currLevel - 1] = file->fileString.substr(((*(*prevPListArray)[i])[0] - (levelInfo.currLevel-1)), levelInfo.currLevel);
					}
				}
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
			file->fileString.resize(0);
		}

		typedef std::map<PatternType, PListType>::iterator it_map_type;

		map<PatternType, PListType> finalMetaDataMap;
		vector<PListArchive*> archiveCollection;
		//Divide between file load and previous level pLists and leave some for new lists haha 
		unsigned long long memDivisor = (memoryPerThread*1000000)/3.0f;

		int currentFile = 0;
		bool memoryOverflow = false;
		vector<string> temp;
		if(prediction)
		{
			if(levelToOutput == 0 || (levelToOutput != 0 && globalLevel >= levelToOutput))
			{
				ProcessChunksAndGenerate(backupFilenames, temp, memDivisor, 0, 1, true);
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
			
		//stringstream buff;
		//buff << "Level/tPatterns/tMost Common Pattern/t" << j + 1 << " count is " << levelRecordings[j] << endl;
		
		for(int j = 0; j < levelRecordings.size() && levelRecordings[j] != 0; j++)
		{
			
			stringstream buff;
			buff << "Level " << j + 1 << " count is " << levelRecordings[j] << " with most common pattern being: \"" << mostCommonPattern[j] << "\" occured " << mostCommonPatternCount[j] << endl;
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

	processingFinished = true;

	delete file;
	
	delete threadPool;
	delete threadPlantSeedPoolHD;
	delete threadPlantSeedPoolRAM;

	delete countMutex;
	delete prevPListArray;
	delete globalPListArray;

	initTime.Stop();
	initTime.Display();

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
			maxEnter = true;
			i++;
		}
		else if (arg.compare("-max") == 0)
		{
			// We know the next argument *should* be the maximum pattern to display
			maximum = atoi(argv[i + 1]);
			levelRecordings.resize(maximum);
			mostCommonPatternCount.resize(maximum);
			mostCommonPattern.resize(maximum);
			minEnter = true;
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
	cout << "Level " << levelInfo.currLevel << " had " << sizeOfPrevPatternCount << " patterns!" << endl;

	cout << "Eradicated file indexes: " << eradicatedPatterns << endl;
	if(potentialPatterns > file->fileStringSize - (levelInfo.currLevel - 1))
	{
		//Factor in eradicated patterns because those places no longer need to be checked in the file
		potentialPatterns = (file->fileStringSize - eradicatedPatterns) - (levelInfo.currLevel - 1);
		cout << "Potential patterns has exceed file size so we downgrade to " << potentialPatterns << endl;
	}

	cout << "Potential patterns for level " << levelInfo.currLevel << " is " << potentialPatterns << endl;

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

	cout << "Predicted size for level " << levelInfo.currLevel << " is " << predictedMemoryForNextLevelMB << " MB" << endl;
	
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

	cout << "Size used for previous level " << levelInfo.currLevel - 1 << " is " << previousLevelMemoryMB << " MB" << endl;
	
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
		
		stringstream stringbuilder;
		stringbuilder << "Using RAM! Total size for level " << levelInfo.currLevel << " processing is " << predictedMemoryForLevelProcessing << " MB" << endl;
		cout << stringbuilder.str();
		Logger::WriteLog(stringbuilder.str());
		
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

				string fileNameage(threadFilesNames.str());
				fileNameage.insert(0, ARCHIVE_FOLDER);
				fileNameage.append(".txt");
				ofstream outputFile(fileNameage);
				outputFile.close();

				threadFiles.push_back(new PListArchive(threadFilesNames.str()));
				fileList[a].push_back(threadFilesNames.str());
			}
			for(PListType prevIndex = 0; prevIndex < prevLocalPListArray->size(); prevIndex++)
			{
				if((*prevLocalPListArray)[prevIndex] != NULL)
				{
					threadFiles[threadNumber]->WriteArchiveMapMMAP((*prevLocalPListArray)[prevIndex]);
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
				threadFiles[a]->WriteArchiveMapMMAP(NULL, "", true);
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
						vector<vector<PListType>*>* packedPListArray = archive.GetPListArchiveMMAP(10000000.0f);
						prevLocalPListArray->insert(prevLocalPListArray->end(), packedPListArray->begin(), packedPListArray->end());
						
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
				
			fileIDMutex->lock();
			fileID++;
			threadFilesNames.str("");
			threadFilesNames << "PListChunks" << fileID;
			fileIDMutex->unlock();

			string fileNameage(threadFilesNames.str());
			fileNameage.insert(0, ARCHIVE_FOLDER);
			fileNameage.append(".txt");
			ofstream outputFile(fileNameage);
			outputFile.close();

			
			stringstream stringbuilder;
			stringbuilder << "New file being generated : " << fileNameage << endl;
			Logger::WriteLog(stringbuilder.str());
			

			threadFile = new PListArchive(threadFilesNames.str());
			fileList.push_back(threadFilesNames.str());
				
			for(PListType prevIndex = 0; prevIndex < prevLocalPListArray->size(); prevIndex++)
			{
				threadFile->WriteArchiveMapMMAP((*prevLocalPListArray)[prevIndex]);
				delete (*prevLocalPListArray)[prevIndex];
			}
			//Clear out the array also after deletion
			prevLocalPListArray->clear();
			
			threadFile->WriteArchiveMapMMAP(NULL, "", true);
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
					vector<vector<PListType>*>* packedPListArray = archive.GetPListArchiveMMAP(10000000.0f);
					prevLocalPListArray->insert(prevLocalPListArray->end(), packedPListArray->begin(), packedPListArray->end());
						
					packedPListArray->erase(packedPListArray->begin(), packedPListArray->end());
					delete packedPListArray;
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
			/*threadPool->push_back(std::async(std::launch::async, &Forest::ThreadedLevelTreeSearchRecursionList, this, prevPListArray, temp, fileList[i], levelInfo));*/
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

			string fileNameage(threadFilesNames.str());
			fileNameage.insert(0, ARCHIVE_FOLDER);
			fileNameage.append(".txt");
			ofstream outputFile(fileNameage);
			outputFile.close();

			threadFiles.push_back(new PListArchive(threadFilesNames.str()));
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
				vector<vector<PListType>*>* packedPListArray = archive.GetPListArchiveMMAP(10000000.0f);

				for(PListType prevIndex = 0; prevIndex < packedPListArray->size(); prevIndex++)
				{
					threadFiles[threadNumber]->WriteArchiveMapMMAP((*packedPListArray)[prevIndex]);
					delete (*packedPListArray)[prevIndex];
					
					//Increment chunk
					threadNumber++;
					if(threadNumber >= threadsToDispatch)
					{
						threadNumber = 0;
					}
				}
							
				packedPListArray->erase(packedPListArray->begin(), packedPListArray->end());
				delete packedPListArray;
			}
			archive.CloseArchiveMMAP();
			//Now delete it
			DeleteArchive(prevFileNames[prevChunkCount], ARCHIVE_FOLDER);
		
		}

		for(int a = 0; a < threadsToDispatch; a++)
		{
			threadFiles[a]->WriteArchiveMapMMAP(NULL, "", true);
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
		fileNameage.insert(0, ARCHIVE_FOLDER);
		fileNameage.append(".txt");
		ofstream outputFile(fileNameage);
		outputFile.close();
		MemoryUtils::copyFileOverBackup(fileNamesToReOpen[a], newFileName);
		fileNamesBackup.push_back(newFileName);

		patternNameToCopyFrom.append("Patterns");

		string fileNameage2(patternName);
		fileNameage2.insert(0, ARCHIVE_FOLDER);
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

			if(overMemoryCount && !memoryOverflow)
			{
				memoryOverflow = true;
				currentFile = a;
			}
			else if(overMemoryCount && memoryOverflow)
			{
				//wait for one second for other memory to clear up
				//std::this_thread::sleep_for (std::chrono::seconds(1));
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
				countMutex->lock();
				eradicatedPatterns++;
				countMutex->unlock();
			}
		}
	}

	//Delete unused backup files 
	for(int a = 0; a < fileNamesBackup.size(); a++)
	{
		DeleteChunks(fileNamesBackup, ARCHIVE_FOLDER);
	}

	return interimCount;
}


//PListType Forest::ProcessChunksAndGenerate(vector<string> fileNamesToReOpen, vector<string>& newFileNames, PListType memDivisor, unsigned int threadNum, unsigned int currLevel, bool firstLevel)
//{
//	int currentFile = 0;
//	int prevCurrentFile = currentFile;
//	bool memoryOverflow = false;
//	PListType interimCount = 0;
//	unsigned int threadNumber = 0;
//	unsigned int threadsToDispatch = numThreads - 1;
//
//	
//	vector<string> fileNamesBackup;
//	for(int a = 0; a < fileNamesToReOpen.size(); a++)
//	{
//		fileNamesBackup.push_back(fileNamesToReOpen[a]);
//	}
//
//	vector<PListArchive*> patternFiles;
//
//	while(currentFile < fileNamesBackup.size())
//	{
//		memoryOverflow = false;
//
//		vector<PListArchive*> archiveCollection;
//		map<PatternType, PListArchive*> finalMetaDataMap;
//
//		for(int a = currentFile; a < fileNamesBackup.size(); a++)
//		{
//			archiveCollection.push_back(new PListArchive(fileNamesBackup[a]));
//		}
//			
//		for(int a = 0; a < archiveCollection.size(); a++)
//		{
//			
//			//Our job is to trust whoever made the previous chunk made it within the acceptable margin of error so we compensate by saying take up to double the size if the previous
//			//chunk went a little over the allocation constraint
//			//cout << "Processing file : " << archiveCollection[a]->fileName << endl;
//			vector<vector<PListType>*>* packedPListArray = archiveCollection[a]->GetPListArchiveMMAP(/*2.0f*memDivisor/1000000.0*/); //Needs MB
//			PListType packedPListSize = packedPListArray->size();
//
//			std::string::size_type i = archiveCollection[a]->fileName.find(".txt");
//			string tempString = archiveCollection[a]->fileName;
//			tempString.erase(i, 8);
//			tempString.erase(0, 7);
//			string fileNameForLater;
//			fileNameForLater.append(tempString);
//			string fileName(fileNameForLater);
//			fileName.append("Patterns");
//			PListArchive *stringBufferFile = new PListArchive(fileName);
//			vector<string> *stringBuffer = stringBufferFile->GetPatterns(currLevel, packedPListArray->size());
//
//			if(MemoryUtils::IsOverMemoryCount(MemoryUsedPriorToThread, memoryBandwidthMB) && !memoryOverflow && !firstLevel)
//			{
//				memoryOverflow = true;
//			}
//			else if(MemoryUtils::IsOverMemoryCount(MemoryUsedPriorToThread, memoryBandwidthMB) && memoryOverflow)
//			{
//				//wait for one second for other memory to clear up
//				std::this_thread::sleep_for (std::chrono::seconds(1));
//			}
//				
//			for(PListType partialLists = 0; partialLists < packedPListArray->size(); partialLists++)
//			{
//				try
//				{
//					string pattern = (*stringBuffer)[partialLists];
//					//This allows us to back fill the others iterations when this didn't have a pattern
//					if(finalMetaDataMap.find(pattern) == finalMetaDataMap.end())
//					{
//						if(!memoryOverflow)
//						{
//							fileIDMutex->lock();
//							fileID++;
//							stringstream name;
//							name << fileID;
//							stringstream stream;
//							stream << ARCHIVE_FOLDER << fileID << ".txt";
//							fileIDMutex->unlock();
//							ofstream outputFile(stream.str());
//							outputFile.close();
//
//							finalMetaDataMap[pattern] = new PListArchive(name.str());
//
//							//finalMetaDataMap[pattern]->WriteArchiveMapMMAP((*packedPListArray)[partialLists], "", true);
//							finalMetaDataMap[pattern]->WriteArchiveMapMMAP((*packedPListArray)[partialLists], (*stringBuffer)[partialLists]);
//							
//							//delete (*packedPListArray)[partialLists];
//							(*packedPListArray)[partialLists] = NULL;
//
//							finalMetaDataMap[pattern]->CloseArchiveMMAP();
//						}
//					}
//					else
//					{
//						finalMetaDataMap[pattern]->OpenArchiveMMAP();
//						//finalMetaDataMap[pattern]->WriteArchiveMapMMAP((*packedPListArray)[partialLists], "", true);
//						finalMetaDataMap[pattern]->WriteArchiveMapMMAP((*packedPListArray)[partialLists], (*stringBuffer)[partialLists]);
//						//delete (*packedPListArray)[partialLists];
//						(*packedPListArray)[partialLists] = NULL;
//						finalMetaDataMap[pattern]->CloseArchiveMMAP();
//					}
//						
//				}
//				catch(exception e)
//				{
//					cout << "System exception: " << e.what() << endl;
//				}
//			}
//
//			if(!memoryOverflow)
//			{
//				currentFile++;
//			}
//				
//			archiveCollection[a]->DumpContents();
//			archiveCollection[a]->CloseArchiveMMAP();
//
//			delete archiveCollection[a];
//
//			stringBufferFile->DumpContents();
//			stringBufferFile->CloseArchiveMMAP();
//
//			delete stringBufferFile;
//
//			string backupFile = fileNameForLater;
//			PListArchive* archiveCollective = new PListArchive(backupFile);
//			for(PListType partialLists = 0; partialLists < packedPListSize; partialLists++)
//			{
//				if((*packedPListArray)[partialLists] != NULL)
//				{
//					archiveCollective->WriteArchiveMapMMAP((*packedPListArray)[partialLists], (*stringBuffer)[partialLists]);
//					delete (*packedPListArray)[partialLists];
//					(*packedPListArray)[partialLists] = NULL;
//				}
//			}
//
//			//cout << "file name: " << archiveCollective->fileName << " size of new plist buffer: " << pListNewBufferCount << " size of new string buffer: " << archiveCollective->stringBuffer.size() << endl;
//
//			archiveCollective->WriteArchiveMapMMAP(NULL, "", true);
//			archiveCollective->CloseArchiveMMAP();
//
//			archiveCollective->DumpPatternsToDisk(currLevel);
//
//			delete archiveCollective;
//
//			delete packedPListArray;
//
//			stringBuffer->clear();
//			delete stringBuffer;
//		}
//
//		PListType tempMemoryMB = MemoryUtils::GetProgramMemoryConsumption();
//		PListType memDivisorMB = memDivisor/1000000.0f;
//
//		//thread files
//		PListArchive* currChunkFile = NULL;
//		bool notBegun = true;
//		PListType removedPatterns = 0;
//
//		for(it_map_plistarchive_type iterator = finalMetaDataMap.begin(); iterator != finalMetaDataMap.end(); iterator++)
//		{
//			iterator->second->OpenArchiveMMAP();
//			iterator->second->WriteArchiveMapMMAP(NULL, "", true);
//			iterator->second->CloseArchiveMMAP();
//			iterator->second->fileIndex = 0;
//			iterator->second->startingIndex = 0;
//			iterator->second->mappingIndex = 0;
//		}
//
//		for(it_map_plistarchive_type iterator = finalMetaDataMap.begin(); iterator != finalMetaDataMap.end(); iterator++)
//		{
//			if(MemoryUtils::IsLessThanMemoryCount(tempMemoryMB, memDivisorMB) || notBegun)
//			{
//				notBegun = false;
//				if(currChunkFile != NULL)
//				{
//					currChunkFile->WriteArchiveMapMMAP(NULL, "", true);
//					currChunkFile->CloseArchiveMMAP();
//					delete currChunkFile;
//				}
//				
//				if(firstLevel)
//				{
//					stringstream fileNameage;
//					fileIDMutex->lock();
//					fileID++;
//					fileNameage << ARCHIVE_FOLDER << "PListChunks" << fileID << ".txt";
//					stringstream fileNameForPrevList;
//					fileNameForPrevList << "PListChunks" << fileID;
//					fileIDMutex->unlock();
//					 
//					ofstream outputFile(fileNameage.str());
//					outputFile.close();
//	
//					prevFileNameList[threadNumber].push_back(fileNameForPrevList.str());
//				
//					currChunkFile = new PListArchive(fileNameForPrevList.str());
//					//After dumping file to memory lets get a new curr memory
//					tempMemoryMB = MemoryUtils::GetProgramMemoryConsumption();
//	
//					threadNumber++;
//					threadNumber %= threadsToDispatch;
//				}
//				else
//				{
//					stringstream fileNameage;
//					fileIDMutex->lock();
//					fileID++;
//					fileNameage << ARCHIVE_FOLDER << "PListChunks" << fileID << ".txt";
//					stringstream fileNameForPrevList;
//					fileNameForPrevList << "PListChunks" << fileID;
//					fileIDMutex->unlock();
//					
//					ofstream outputFile(fileNameage.str());
//					outputFile.close();
//	
//					newFileNames.push_back(fileNameForPrevList.str());
//				
//					currChunkFile = new PListArchive(fileNameForPrevList.str());
//					//After dumping file to memory lets get a new curr memory
//					tempMemoryMB = MemoryUtils::GetProgramMemoryConsumption();
//				}
//			}
//			
//			if(iterator->second != NULL)
//			{
//				iterator->second->OpenArchiveMMAP();
//				vector<vector<PListType>*>* list = iterator->second->GetPListArchiveMMAP();
//				if((*list).size() > 0 && (*list)[0]->size() > 1)
//				{
//					
//					currChunkFile->WriteArchiveMapMMAP((*list)[0]);
//					
//					interimCount++;
//
//					if(mostCommonPattern.size() < currLevel)
//					{
//						mostCommonPattern.resize(currLevel);
//						mostCommonPatternCount.resize(currLevel);
//					}
//				
//					if((*list)[0]->size() > mostCommonPatternCount[currLevel - 1])
//					{
//						mostCommonPatternCount[currLevel - 1] = (*list)[0]->size();
//						mostCommonPattern[currLevel - 1] = iterator->first;
//					}
//				}
//				iterator->second->CloseArchiveMMAP();
//				
//				/*stringstream builder;
//				builder << "Pattern " << (*iterator).first << " occurs " << iterator->second->size() << " using thread " << threadNum << " written to " << currChunkFile->fileName << endl;
//				Logger::WriteLog(builder.str());*/
//			}
//			else
//			{
//				removedPatterns++;
//			}
//
//			//iterator->second->CloseArchiveMMAP();
//			DeleteArchive(iterator->second->patternName, ARCHIVE_FOLDER);
//			
//			delete iterator->second;
//		}
//
//		if(currChunkFile != NULL)
//		{
//			currChunkFile->WriteArchiveMapMMAP(NULL, "", true);
//			currChunkFile->CloseArchiveMMAP();
//			delete currChunkFile;
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
//
//		//cout << "Thread " << threadNum << " has encountered " << interimCount << " patterns!" << endl;
//	}
//
//	countMutex->lock();
//	if(levelRecordings.size() < currLevel)
//	{
//		levelRecordings.resize(currLevel);
//	}
//	levelRecordings[currLevel - 1] += interimCount;
//	cout << currLevel << " with a total of " << levelRecordings[currLevel - 1] << endl;
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

	vector<PListArchive*> patternFiles;

	while(currentFile < fileNamesBackup.size())
	{
		memoryOverflow = false;

		vector<PListArchive*> archiveCollection;
		map<PatternType, vector<PListType>*> finalMetaDataMap;

		for(int a = currentFile; a < fileNamesBackup.size(); a++)
		{
			archiveCollection.push_back(new PListArchive(fileNamesBackup[a]));
		}
			
		for(int a = 0; a < archiveCollection.size(); a++)
		{
			
			//Our job is to trust whoever made the previous chunk made it within the acceptable margin of error so we compensate by saying take up to double the size if the previous
			//chunk went a little over the allocation constraint
			
			vector<vector<PListType>*>* packedPListArray = NULL;
			vector<string> *stringBuffer = NULL;
			PListArchive *stringBufferFile = NULL;
			string fileNameForLater ="";
			PListType packedPListSize = 0; 
			string fileName ="";
			bool foundAHit = true;
			

			if(overMemoryCount && !memoryOverflow/* && !firstLevel*/)
			{
				stringstream crap;
				crap << "Overflow at Process Chunks And Generate of " << currMemoryOverflow << " in MB!\n";
				Logger::WriteLog(crap.str());
				memoryOverflow = true;
			}
			else if(overMemoryCount && memoryOverflow)
			{
				stringstream crap;
				crap << "Overflow at Process Chunks And Generate of " << currMemoryOverflow << " in MB!\n";
				Logger::WriteLog(crap.str());
				//wait for one second for other memory to clear up
				//std::this_thread::sleep_for (std::chrono::seconds(1));
			}

			if(!memoryOverflow)
			{
				packedPListArray = archiveCollection[a]->GetPListArchiveMMAP(); //Needs MB
				packedPListSize = packedPListArray->size();

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
				
			}
			else
			{
				
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
					packedPListArray = archiveCollection[a]->GetPListArchiveMMAP(); //Needs MB
					
				}
				

			}
			
			PListType countAdded = 0;
			if(foundAHit)
			{
  				for(PListType partialLists = 0; partialLists < packedPListArray->size(); partialLists++)
				{
					try
					{
						
						if(overMemoryCount && !memoryOverflow)
						{
							memoryOverflow = true;
						}

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
								countAdded++;
								std::string().swap((*stringBuffer)[partialLists]);
							}
						}
						else
						{
							finalMetaDataMap[pattern]->insert(finalMetaDataMap[pattern]->end(), (*packedPListArray)[partialLists]->begin(), (*packedPListArray)[partialLists]->end());
							delete (*packedPListArray)[partialLists];
							(*packedPListArray)[partialLists] = NULL;
							countAdded++;
							std::string().swap((*stringBuffer)[partialLists]);
						}
						
					}
					catch(exception e)
					{
						cout << "System exception: " << e.what() << endl;
					}
				}
			}

			/*if(overMemoryCount)
			{
				stringstream crap;
				crap << "Overflow at Process Chunks And Generate during map allocation of " << currMemoryOverflow << " in MB!\n";
				Logger::WriteLog(crap.str());
				memoryOverflow = true;
			}*/

			
				
			if(foundAHit)
			{
				if(countAdded != packedPListSize)
				{

					archiveCollection[a]->CloseArchiveMMAP();
					stringBufferFile->CloseArchiveMMAP();

					DeleteChunk(fileNameForLater, ARCHIVE_FOLDER);
					
					delete archiveCollection[a];
					delete stringBufferFile;

					string backupFile = fileNameForLater;
					std::string::size_type j = backupFile.find("_");
					backupFile.erase(j + 1, backupFile.size() - j);

					PListType newCount = packedPListSize - countAdded;
					stringstream buffy;
					buffy << backupFile << newCount;
					string temp = buffy.str();
					fileNamesBackup[prevCurrentFile + a] = temp;
					temp.insert(0, ARCHIVE_FOLDER);
					temp.append(".txt");
					ofstream outputFile(temp);
					outputFile.close();

					PListType testCount = 0;
					PListArchive* archiveCollective = new PListArchive(fileNamesBackup[prevCurrentFile + a]);
					for(PListType partialLists = 0; partialLists < packedPListSize; partialLists++)
					{
						if((*packedPListArray)[partialLists] != NULL)
						{
							archiveCollective->WriteArchiveMapMMAP((*packedPListArray)[partialLists], (*stringBuffer)[partialLists]);
							delete (*packedPListArray)[partialLists];
							(*packedPListArray)[partialLists] = NULL;
							testCount++;
						}
					}

					//cout << "file name: " << archiveCollective->fileName << " size of new plist buffer: " << pListNewBufferCount << " size of new string buffer: " << archiveCollective->stringBuffer.size() << endl;

					archiveCollective->WriteArchiveMapMMAP(NULL, "", true);
					archiveCollective->CloseArchiveMMAP();

					archiveCollective->DumpPatternsToDisk(currLevel);

					delete archiveCollective;
				}
				else
				{
					archiveCollection[a]->DumpContents();
					archiveCollection[a]->CloseArchiveMMAP();

					delete archiveCollection[a];

					stringBufferFile->DumpContents();
					stringBufferFile->CloseArchiveMMAP();

					delete stringBufferFile;
				}

				delete packedPListArray;
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
			if(!memoryOverflow || countAdded == packedPListSize)
			{
				currentFile++;
			}
		}

		PListType tempMemoryMB = MemoryUtils::GetProgramMemoryConsumption();
		PListType memDivisorMB = memDivisor/1000000.0f;

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
					currChunkFile->WriteArchiveMapMMAP(NULL, "", true);
					currChunkFile->CloseArchiveMMAP();
					delete currChunkFile;
				}
				
				if(firstLevel)
				{
					stringstream fileNameage;
					fileIDMutex->lock();
					fileID++;
					fileNameage << ARCHIVE_FOLDER << "PListChunks" << fileID << ".txt";
					stringstream fileNameForPrevList;
					fileNameForPrevList << "PListChunks" << fileID;
					fileIDMutex->unlock();
					 
					ofstream outputFile(fileNameage.str());
					outputFile.close();
	
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
					fileIDMutex->lock();
					fileID++;
					fileNameage << ARCHIVE_FOLDER << "PListChunks" << fileID << ".txt";
					stringstream fileNameForPrevList;
					fileNameForPrevList << "PListChunks" << fileID;
					fileIDMutex->unlock();
					
					ofstream outputFile(fileNameage.str());
					outputFile.close();
	
					newFileNames.push_back(fileNameForPrevList.str());
				
					currChunkFile = new PListArchive(fileNameForPrevList.str());
					//After dumping file to memory lets get a new curr memory
					tempMemoryMB = MemoryUtils::GetProgramMemoryConsumption();
				}
			}
			
			if(iterator->second->size() > 1)
			{
				currChunkFile->WriteArchiveMapMMAP(iterator->second);
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
				
				
				/*stringstream builder;
				builder << "Pattern " << (*iterator).first << " occurs " << iterator->second->size() << " using thread " << threadNum << " written to " << currChunkFile->fileName << endl;
				Logger::WriteLog(builder.str());*/
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

		for(int a = prevCurrentFile; a < currentFile; a++)
		{
			DeleteChunk(fileNamesBackup[a], ARCHIVE_FOLDER);
		}
		prevCurrentFile = currentFile;

		//cout << "Thread " << threadNum << " has encountered " << interimCount << " patterns!" << endl;
	}

	countMutex->lock();
	if(levelRecordings.size() < currLevel)
	{
		levelRecordings.resize(currLevel);
	}
	levelRecordings[currLevel - 1] += interimCount;
	
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


TreeHD Forest::RAMToHDLeafConverter(TreeRAM leaf)
{
	TreeHD tree;
	try
	{
	
		for(PListType i = 0; i < leaf.leaves.size(); i++)
		{
			if(leaf.leaves[i] != NULL)
			{
				PListType pListSize = leaf.leaves[i]->GetPListCount();
				vector<PListType>* pList = leaf.leaves[i]->GetPList();
				for(PListType j = 0; j < pListSize; j++)
				{
					if((*pList)[j] < file->fileString.length())
					{
						tree.addLeaf((*pList)[j], file->fileString.substr(((*pList)[j] - (globalLevel-1)), globalLevel));
					}
				}
				delete leaf.leaves[i];
			}
		}
	}
	catch(exception e)
	{
		cout << e.what() << endl;
	}

	return tree;
}

//bool Forest::ProcessHD(LevelPackage& levelInfo, vector<string>& fileList, bool &isThreadDefuncted)
//{
//
//	int threadNum = levelInfo.threadIndex;
//	int currLevel = levelInfo.currLevel;
//	PListType newPatternCount = 0;
//	//Divide between file load and previous level pLists and leave some for new lists haha 
//	unsigned long long memDivisor = (memoryPerThread*1000000)/3.0f;
//		
//	bool morePatternsToFind = false;
//
//	unsigned long long fileIters = file->fileStringSize/memDivisor;
//	if(file->fileStringSize%memDivisor != 0)
//	{
//		fileIters++;
//	}
//
//	typedef std::vector<map<PatternType, PListType>>::iterator it_type;
//	typedef std::map<PatternType, PListType>::iterator it_map_type;
//	typedef std::map<PatternType, vector<PListType>>::iterator it_map_list_type;
//
//	string fileChunkData;
//	vector<string> fileNamesToReOpen;
//	string saveOffPreviousStringData = "";
//	PListType prevPatternTally = 0;
//	vector<string> newFileNames;
//	bool localUsingRAM = globalUsingRAM;
//	try
//	{
//		for(PListType prevChunkCount = 0; prevChunkCount < fileList.size(); prevChunkCount++)
//		{
//			PListArchive archive(fileList[prevChunkCount]);
//			
//			while(!archive.IsEndOfFile())
//			{
//				vector<vector<PListType>*>* packedPListArray = archive.GetPListArchiveMMAP(/*memDivisor/1000000.0f*/); //Needs MB
//				PListType packedListSize = packedPListArray->size();
//				prevPatternTally += packedListSize;
//				vector <PListType> prevLeafIndex(packedListSize, 0);
//		
//				FileReader fileReaderTemp(file->fileName);
//				
//				for(int j = 0;j < fileIters; j++)
//				{
//					if(packedListSize > 0)
//					{
//						
//						if(fileChunkData.size() > 0)
//						{
//							saveOffPreviousStringData = fileChunkData.substr(fileChunkData.size() - (currLevel - 1), currLevel - 1);
//						}
//
//						PListType patternCount = 0;
//						if(file->fileStringSize <= memDivisor*j + memDivisor)
//						{
//							patternCount = file->fileStringSize - memDivisor*j;
//						}
//						else
//						{
//							patternCount = memDivisor;
//						}
//
//						fileChunkData.clear();
//						fileChunkData = "";
//						fileChunkData.resize(patternCount);
//						fileReaderTemp.copyBuffer->read( &fileChunkData[0], patternCount);
//
//					}
//
//					TreeHD leaf;
//					bool justPassedMemorySize = false;
//
//					for(PListType i = 0; i < packedListSize; i++)
//					{
//						vector<PListType>* pList = (*packedPListArray)[i];
//						PListType pListLength = (*packedPListArray)[i]->size();
//						PListType k = prevLeafIndex[i];
//
//						if(!overMemoryCount)
//						{
//							while( k < pListLength && ((*pList)[k]) < (j+1)*memDivisor )
//							{
//								try
//								{
//									if(((*pList)[k]) < file->fileStringSize)
//									{
//										//If index comes out to be larger than fileString than it is a negative number 
//										//and we must use previous string data!
//										if(((((*pList)[k]) - memDivisor*j) - (currLevel-1)) >= file->fileStringSize)
//										{
//											signed long long relativeIndex = ((((*pList)[k]) - memDivisor*j) - (currLevel-1));
//											string pattern = "";
//											relativeIndex *= -1;
//											PListType indexForString = saveOffPreviousStringData.size() - relativeIndex;
//											if(saveOffPreviousStringData.size() > 0 && relativeIndex > 0)
//											{
//												pattern = saveOffPreviousStringData.substr(indexForString, relativeIndex);
//												pattern.append(fileChunkData.substr(0, currLevel - pattern.size()));
//												leaf.addLeaf((*pList)[k]+1, pattern);
//											}
//											
//											//cout << "string over the border: " << saveOffPreviousStringData << endl;
//											//cout << "Relative index: " << relativeIndex << " Index for string: " << indexForString << " pattern: " << pattern << " is size: " << pattern.size() << endl;
//										}
//										else
//										{
//											//If pattern is past end of string stream then stop counting this pattern
//											if(((*pList)[k]) < file->fileStringSize)
//											{
//												leaf.addLeaf((*pList)[k]+1, fileChunkData.substr(((((*pList)[k]) - memDivisor*j) - (currLevel-1)), currLevel));
//											}
//											else if(((((*pList)[k]) - memDivisor*j) - (currLevel-1)) < 0)
//											{
//												cout << "String access is out of bounds at beginning" << endl;
//											}
//											else if((((*pList)[k]) - memDivisor*j) >= file->fileStringSize)
//											{
//												cout << "String access is out of bounds at end" << endl;
//											}
//										}
//
//									}
//									else
//									{
//										cout << "don't pattern bro at this index: " << ((*pList)[k]) << endl;
//									}
//								}
//								catch(exception e)
//								{
//									cout << "Exception at global index: " << (*pList)[k] << "Exception at relative index: " << ((((*pList)[k]) - memDivisor*j) - (currLevel-1)) << "    System exception: " << e.what() << endl;
//								}
//								k++;
//							}
//							prevLeafIndex[i] = k;
//							justPassedMemorySize = false;
//						}
//						else
//						{
//							//if true already do not write again until memory is back in our hands
//							if(!justPassedMemorySize && leaf.leaves.size() > 0)
//							{
//								
//								//cout << "Prior memory to dump: " << MemoryUtils::GetProgramMemoryConsumption() << endl;
//
//								PListType patterns = leaf.leaves.size();
//								//cout << "Pattern count: " << patterns << endl;
//								PListType sizeOfTreeMap = sizeof(char)*currLevel*patterns + sizeof(TreeHD)*patterns;
//								PListType currentTreeSizeMB = (patterns*sizeof(PListType) + sizeOfTreeMap)/1000000.0f;
//								sizeOfPreviousLevelMB += currentTreeSizeMB;
//								
//								justPassedMemorySize = true;
//								stringstream stringBuilder;
//								fileIDMutex->lock();
//								fileID++;
//								stringBuilder << fileID;
//								fileIDMutex->unlock();
//								fileNamesToReOpen.push_back(CreateChunkFile(stringBuilder.str(), leaf, threadNum, currLevel));
//
//								//cout << "Memory after dump: " << MemoryUtils::GetProgramMemoryConsumption() << endl;
//								
//							}
//							else
//							{
//								
//								//If memory is unavailable sleep for one second
//								std::this_thread::sleep_for (std::chrono::seconds(1));
//							}
//							i--;
//						}
//					}
//
//					if(packedListSize > 0 && leaf.leaves.size() > 0)
//					{
//						//cout << "Prior memory to dump: " << MemoryUtils::GetProgramMemoryConsumption() << endl;
//
//						PListType patterns = leaf.leaves.size();
//						//cout << "Pattern count: " << patterns << endl;
//						PListType sizeOfTreeMap = sizeof(char)*currLevel*patterns + sizeof(TreeHD)*patterns;
//						PListType currentTreeSizeMB = (patterns*sizeof(PListType) + sizeOfTreeMap)/1000000.0f;
//						//cout << "Dumping off a leaf load of " << patterns << " patterns at level " << currLevel << endl;
//						sizeOfPreviousLevelMB += currentTreeSizeMB;
//
//						stringstream stringBuilder;
//						fileIDMutex->lock();
//						fileID++;
//						stringBuilder << fileID;
//						fileIDMutex->unlock();
//						fileNamesToReOpen.push_back(CreateChunkFile(stringBuilder.str(), leaf, threadNum, currLevel));
//						delete leaf.GetPList();
//
//						//cout << "Memory after dump: " << MemoryUtils::GetProgramMemoryConsumption() << endl;
//					}
//				}
//				
//		
//				for(PListType pTits = 0; pTits < packedPListArray->size(); pTits++)
//				{
//					delete (*packedPListArray)[pTits];
//				}
//				delete packedPListArray;
//
//			}
//			archive.CloseArchiveMMAP();
//		}
//
//		fileChunkData.clear();
//		fileChunkData = "";
//
//		if(levelToOutput == 0 || (levelToOutput != 0 && currLevel >= levelToOutput))
//		{
//			newPatternCount += ProcessChunksAndGenerate(fileNamesToReOpen, newFileNames, memDivisor, threadNum, currLevel);
//		}
//	}
//	catch(exception e)
//	{
//		cout << e.what() << endl;
//		MemoryUtils::print_trace();
//	}
//
//	if(!history)
//	{
//		DeleteArchives(fileList, ARCHIVE_FOLDER);
//	}
//
//
//	fileList.clear();
//	for(int i = 0; i < newFileNames.size(); i++)
//	{
//		string fileNameToBeRemoved = ARCHIVE_FOLDER;
//		fileNameToBeRemoved.append(newFileNames[i].c_str());
//		fileNameToBeRemoved.append(".txt");
//		
//		PListType fileSizeOfChunk = MemoryUtils::FileSize(fileNameToBeRemoved);
//		if(fileSizeOfChunk > 0)
//		{
//			fileList.push_back(newFileNames[i]);
//		}
//		else
//		{
//			if( remove( fileNameToBeRemoved.c_str() ) != 0 && VERBOSITY_LEVEL)
//			{
//				stringstream builder;
//				builder << "Adding new failed to delete " << fileNameToBeRemoved << ": " << strerror(errno) << '\n';
//				Logger::WriteLog(builder.str());
//			}
//			else if(VERBOSITY_LEVEL)
//			{
//				stringstream builder;
//				builder << "Adding new succesfully deleted " << fileNameToBeRemoved << '\n';
//				Logger::WriteLog(builder.str());
//			}
//		}
//	}
//	
//	newFileNames.clear();
//
//	if(fileList.size() > 0 && levelInfo.currLevel <= maximum)
//	{
//		morePatternsToFind = true;
//		levelInfo.currLevel++;
//
//		/*****************NEW CODE***************************/
//
//
//
//
//		bool alreadyUnlocked = false;
//		countMutex->lock();
//
//		int threadsToDispatch = numThreads - 1;
//		int unusedCores = (threadsToDispatch - (threadsDispatched - threadsDefuncted)) + 1;
//		if(newPatternCount < unusedCores && unusedCores > 1)
//		{
//			unusedCores = newPatternCount;
//		}
//		//Need to have an available core, need to still have patterns to search and need to have more than 1 pattern to be worth splitting up the work
//		if(unusedCores > 1 && morePatternsToFind && newPatternCount > 1)
//		{
//			//OLD WAY THREAD PRORITY//
//			/*unsigned int levelCount = 1000000000;
//			vector<int> threadPriority;
//
//			for(int z = 0; z < currentLevelVector.size(); z++)
//			{
//				if(activeThreads[z])
//				{
//					if(currentLevelVector[z] < levelCount && activeThreads[z])
//					{
//						levelCount = currentLevelVector[z];
//						threadPriority.push_back(z);
//					}
//				}
//			}
//
//			bool spawnThreads = false;
//			for(int z = 0; z < threadPriority.size(); z++)
//			{
//				if(threadPriority[z] == levelInfo.threadIndex && currentLevelVector[threadPriority[z]] == levelCount)
//				{
//					spawnThreads = true;
//				}
//			}*/
//			//END OF OLD WAY THREAD PRORITY//
//			
//			bool spawnThreads = true;
//			//If this thread is at the lowest level of progress spawn new threads
//			if(spawnThreads)
//			{
//				vector<string> tempList = fileList;
//				vector<vector<string>> balancedTruncList = ProcessThreadsWorkLoadHD(unusedCores, levelInfo, tempList);
//				vector<unsigned int> localWorkingThreads;
//				for(unsigned int i = 0; i < balancedTruncList.size(); i++)
//				{
//					localWorkingThreads.push_back(i);
//				}
//
//				if(localWorkingThreads.size() > 1)
//				{
//					int threadsToTest = (threadsDispatched - threadsDefuncted) - 1;
//					if(threadsToTest + localWorkingThreads.size() <= threadsToDispatch)
//					{
//						
//						cout << "Thread " << levelInfo.threadIndex << " has priority and is at level " << levelInfo.currLevel << endl;
//
//						LevelPackage levelInfoRecursion;
//						levelInfoRecursion.currLevel = levelInfo.currLevel;
//						levelInfoRecursion.threadIndex = levelInfo.threadIndex;
//						levelInfoRecursion.inceptionLevelLOL = levelInfo.inceptionLevelLOL + 1;
//						levelInfoRecursion.useRAM = false;
//
//						cout << "Current threads in use: " << threadsDispatched - threadsDefuncted + localWorkingThreads.size() - 1 << endl;
//
//						threadsDefuncted++;
//						isThreadDefuncted = true;
//
//						vector<future<void>> *localThreadPool = new vector<future<void>>();
//						for (PListType i = 0; i < localWorkingThreads.size(); i++)
//						{
//							threadsDispatched++;
//							vector<PListType> temp;
//							localThreadPool->push_back(std::async(std::launch::async, &Forest::ThreadedLevelTreeSearchRecursionList, this, prevPListArray, temp, balancedTruncList[i], levelInfoRecursion));
//						}
//						countMutex->unlock();
//						
//						alreadyUnlocked = true;
//						WaitForThreads(localWorkingThreads, localThreadPool, true);
//
//						localThreadPool->erase(localThreadPool->begin(), localThreadPool->end());
//						(*localThreadPool).clear();
//						delete localThreadPool;
//						morePatternsToFind = false;
//					}
//				}
//			}
//		}
//		if(!alreadyUnlocked)
//		{
//			countMutex->unlock();
//		}
//
//
//		/*****************END NEW CODE***************************/
//
//	}
//	return morePatternsToFind;
//}

bool Forest::ProcessHD(LevelPackage& levelInfo, vector<string>& fileList, bool &isThreadDefuncted)
{

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

	typedef std::vector<map<PatternType, PListType>>::iterator it_type;
	typedef std::map<PatternType, PListType>::iterator it_map_type;
	typedef std::map<PatternType, vector<PListType>>::iterator it_map_list_type;

	string fileChunkData;
	vector<string> fileNamesToReOpen;
	string saveOffPreviousStringData = "";
	PListType prevPatternTally = 0;
	vector<string> newFileNames;
	bool localUsingRAM = globalUsingRAM;
	try
	{
		for(PListType prevChunkCount = 0; prevChunkCount < fileList.size(); prevChunkCount++)
		{
			PListArchive archive(fileList[prevChunkCount]);
			
			while(!archive.IsEndOfFile())
			{
				vector<vector<PListType>*>* packedPListArray = archive.GetPListArchiveMMAP(memDivisor/1000000.0f); //Needs MB
				PListType packedListSize = packedPListArray->size();
				prevPatternTally += packedListSize;
				vector <PListType> prevLeafIndex(packedListSize, 0);
		
				FileReader fileReaderTemp(file->fileName);
				
				for(int j = 0;j < fileIters; j++)
				{
					if(packedListSize > 0)
					{
						
						if(fileChunkData.size() > 0)
						{
							saveOffPreviousStringData = fileChunkData.substr(fileChunkData.size() - (currLevel - 1), currLevel - 1);
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

						fileChunkData.clear();
						fileChunkData = "";
						fileChunkData.resize(patternCount);
						fileReaderTemp.copyBuffer->read( &fileChunkData[0], patternCount);

					}

					TreeHD leaf;
					bool justPassedMemorySize = false;

					for(PListType i = 0; i < packedListSize; i++)
					{
						vector<PListType>* pList = (*packedPListArray)[i];
						PListType pListLength = (*packedPListArray)[i]->size();
						PListType k = prevLeafIndex[i];

						if(!overMemoryCount)
						{
							while( k < pListLength && ((*pList)[k]) < (j+1)*memDivisor )
							{
								try
								{
									if(((*pList)[k]) < file->fileStringSize)
									{
										//If index comes out to be larger than fileString than it is a negative number 
										//and we must use previous string data!
										if(((((*pList)[k]) - memDivisor*j) - (currLevel-1)) >= file->fileStringSize)
										{
											signed long long relativeIndex = ((((*pList)[k]) - memDivisor*j) - (currLevel-1));
											string pattern = "";
											relativeIndex *= -1;
											PListType indexForString = saveOffPreviousStringData.size() - relativeIndex;
											if(saveOffPreviousStringData.size() > 0 && relativeIndex > 0)
											{
												pattern = saveOffPreviousStringData.substr(indexForString, relativeIndex);
												pattern.append(fileChunkData.substr(0, currLevel - pattern.size()));
												leaf.addLeaf((*pList)[k]+1, pattern);
											}
											
											//cout << "string over the border: " << saveOffPreviousStringData << endl;
											//cout << "Relative index: " << relativeIndex << " Index for string: " << indexForString << " pattern: " << pattern << " is size: " << pattern.size() << endl;
										}
										else
										{
											//If pattern is past end of string stream then stop counting this pattern
											if(((*pList)[k]) < file->fileStringSize)
											{
												leaf.addLeaf((*pList)[k]+1, fileChunkData.substr(((((*pList)[k]) - memDivisor*j) - (currLevel-1)), currLevel));
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
									cout << "Exception at global index: " << (*pList)[k] << "Exception at relative index: " << ((((*pList)[k]) - memDivisor*j) - (currLevel-1)) << "    System exception: " << e.what() << endl;
								}
								k++;
							}
							prevLeafIndex[i] = k;
							justPassedMemorySize = false;

							////TESTING NEW CODE

							//morePatternsToFind = true;
							//if(DispatchNewThreads(numThreads - 1, morePatternsToFind, fileList, levelInfo, isThreadDefuncted))
							//{
							//	morePatternsToFind = false;
							//	return false;
							//}
							//morePatternsToFind = false;

							////TESTING END CODE
						}
						else
						{
							stringstream crap;
							crap << "Overflow at Process HD of " << currMemoryOverflow << " in MB!\n";
							Logger::WriteLog(crap.str());
							//if true already do not write again until memory is back in our hands
							if(!justPassedMemorySize && leaf.leaves.size() > 0)
							{
								
								//cout << "Prior memory to dump: " << MemoryUtils::GetProgramMemoryConsumption() << endl;

								PListType patterns = leaf.leaves.size();
								//cout << "Pattern count: " << patterns << endl;
								PListType sizeOfTreeMap = sizeof(char)*currLevel*patterns + sizeof(TreeHD)*patterns;
								PListType currentTreeSizeMB = (patterns*sizeof(PListType) + sizeOfTreeMap)/1000000.0f;
								sizeOfPreviousLevelMB += currentTreeSizeMB;
								
								justPassedMemorySize = true;
								stringstream stringBuilder;
								fileIDMutex->lock();
								fileID++;
								stringBuilder << fileID;
								fileIDMutex->unlock();
								fileNamesToReOpen.push_back(CreateChunkFile(stringBuilder.str(), leaf, threadNum, currLevel));

								//cout << "Memory after dump: " << MemoryUtils::GetProgramMemoryConsumption() << endl;
								
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
						//cout << "Prior memory to dump: " << MemoryUtils::GetProgramMemoryConsumption() << endl;

						PListType patterns = leaf.leaves.size();
						//cout << "Pattern count: " << patterns << endl;
						PListType sizeOfTreeMap = sizeof(char)*currLevel*patterns + sizeof(TreeHD)*patterns;
						PListType currentTreeSizeMB = (patterns*sizeof(PListType) + sizeOfTreeMap)/1000000.0f;
						//cout << "Dumping off a leaf load of " << patterns << " patterns at level " << currLevel << endl;
						sizeOfPreviousLevelMB += currentTreeSizeMB;

						stringstream stringBuilder;
						fileIDMutex->lock();
						fileID++;
						stringBuilder << fileID;
						fileIDMutex->unlock();
						fileNamesToReOpen.push_back(CreateChunkFile(stringBuilder.str(), leaf, threadNum, currLevel));
						delete leaf.GetPList();

						//cout << "Memory after dump: " << MemoryUtils::GetProgramMemoryConsumption() << endl;
					}
				}
				
		
				for(PListType pTits = 0; pTits < packedPListArray->size(); pTits++)
				{
					delete (*packedPListArray)[pTits];
				}
				delete packedPListArray;

			}
			archive.CloseArchiveMMAP();

		}

		fileChunkData.clear();
		fileChunkData = "";

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

	if(!history)
	{
		DeleteArchives(fileList, ARCHIVE_FOLDER);
	}


	fileList.clear();
	for(int i = 0; i < newFileNames.size(); i++)
	{
		string fileNameToBeRemoved = ARCHIVE_FOLDER;
		fileNameToBeRemoved.append(newFileNames[i].c_str());
		fileNameToBeRemoved.append(".txt");
		
		PListType fileSizeOfChunk = MemoryUtils::FileSize(fileNameToBeRemoved);
		if(fileSizeOfChunk > 0)
		{
			fileList.push_back(newFileNames[i]);
		}
		else
		{
			if( remove( fileNameToBeRemoved.c_str() ) != 0)
			{
				stringstream builder;
				builder << "Adding new failed to delete " << fileNameToBeRemoved << ": " << strerror(errno) << '\n';
				Logger::WriteLog(builder.str());
			}
			else
			{
				stringstream builder;
				builder << "Adding new succesfully deleted " << fileNameToBeRemoved << '\n';
				Logger::WriteLog(builder.str());
			}
		}
	}
	
	newFileNames.clear();

	if(fileList.size() > 0 && levelInfo.currLevel <= maximum)
	{
		morePatternsToFind = true;
		levelInfo.currLevel++;

		DispatchNewThreads(newPatternCount, morePatternsToFind, fileList, levelInfo, isThreadDefuncted);

	}
	else
	{
		DeleteArchives(fileList, ARCHIVE_FOLDER);
	}
	return morePatternsToFind;
}

bool Forest::DispatchNewThreads(PListType newPatternCount, bool& morePatternsToFind, vector<string> fileList, LevelPackage levelInfo, bool& isThreadDefuncted)
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
					cout << "Thread " << levelInfo.threadIndex << " has priority and is at level " << levelInfo.currLevel << endl;

					LevelPackage levelInfoRecursion;
					levelInfoRecursion.currLevel = levelInfo.currLevel;
					levelInfoRecursion.threadIndex = levelInfo.threadIndex;
					levelInfoRecursion.inceptionLevelLOL = levelInfo.inceptionLevelLOL + 1;
					levelInfoRecursion.useRAM = false;

					cout << "Current threads in use: " << threadsDispatched - threadsDefuncted + localWorkingThreads.size() - 1 << endl;

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
				unsigned char value = file->fileString[(*pList)[k]];
				leaf.addLeaf(value, (*pList)[k] + 1);
			}
		}
		
			
		PListType removedPatternsTemp = 0;
		vector<vector<PListType>*>* newList = leaf.GetLeafPLists(removedPatternsTemp);
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

	if(globalLocalPListArray->size() == 0 || levelInfo.currLevel > maximum)
	{
		continueSearching = false;
	}
	else
	{
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
				
				mostCommonPattern[levelInfo.currLevel - 1] = file->fileString.substr(((*(*globalLocalPListArray)[i])[0] - (levelInfo.currLevel-1)), levelInfo.currLevel);
			}
		}
		

		if(levelRecordings.size() < levelInfo.currLevel)
		{
			levelRecordings.resize(levelInfo.currLevel);
		}

		//cout << levelInfo.currLevel << " with a total of " << levelRecordings[levelInfo.currLevel] << endl;
		
		levelRecordings[levelInfo.currLevel - 1] += globalLocalPListArray->size();
		
		levelInfo.currLevel++;

		if(levelInfo.currLevel > currentLevelVector[levelInfo.threadIndex])
		{
			currentLevelVector[levelInfo.threadIndex] = levelInfo.currLevel;
		}
		countMutex->unlock();
	}
		
	prevLocalPListArray->clear();
	prevLocalPListArray->swap((*globalLocalPListArray));

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
					cout << "Thread " << levelInfo.threadIndex << " has priority and is at level " << levelInfo.currLevel << endl;

					LevelPackage levelInfoRecursion;
					levelInfoRecursion.currLevel = levelInfo.currLevel;
					levelInfoRecursion.threadIndex = levelInfo.threadIndex;
					levelInfoRecursion.inceptionLevelLOL = levelInfo.inceptionLevelLOL + 1;
					levelInfoRecursion.useRAM = true;

					cout << "Current threads in use: " << threadsDispatched - threadsDefuncted + localWorkingThreads.size() - 1 << endl;

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
	return continueSearching;
}

void Forest::ThreadedLevelTreeSearchRecursionList(vector<vector<PListType>*>* patterns, vector<PListType> patternIndexList, vector<string> fileList, LevelPackage levelInfo)
{
	PListType numPatternsToSearch = patternIndexList.size();
	bool isThreadDefuncted = false;
	cout << "Threads dispatched: " << threadsDispatched << " Threads deported: " << threadsDefuncted << " Threads running: " << threadsDispatched - threadsDefuncted << endl;
	
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
		
		if(overMemoryCount)
		{
			stringstream stringBuilder;
			fileIDMutex->lock();
			fileID++;
			stringBuilder << fileID;
			fileIDMutex->unlock();

			countMutex->lock();
			newFileNameList[threadNum].push_back(CreateChunkFile(stringBuilder.str(), leaf, threadNum, globalLevel));
			countMutex->unlock();

		


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

		countMutex->lock();
		newFileNameList[threadNum].push_back(CreateChunkFile(stringBuilder.str(), leaf, threadNum, globalLevel));
		countMutex->unlock();
	}
	
	
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
		
			vector<PListType>* pList = mapping.find(listToDos[i])->second->GetPList();
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

	delete leaf->GetPList();
	delete leaf;
	return NULL;
}

string Forest::CreateChunkFile(string fileName, TreeHD& leaf, unsigned int threadNum, PListType currLevel)
{
	stringstream archiveName;
	string archiveFileType = "PListChunks";
	string fileNameToReOpen;

	archiveName << ARCHIVE_FOLDER << archiveFileType << fileName << "_" << leaf.leaves.size() << ".txt";
	
	ofstream outputFile(archiveName.str());
	outputFile.close();
	
	archiveName.str("");
	
	archiveName << archiveFileType << fileName << "_" << leaf.leaves.size();
	
	PListArchive* archiveCollective = new PListArchive(archiveName.str());
	fileNameToReOpen = archiveName.str();
			
	typedef std::map<string, TreeHD*>::iterator it_type;
	for(it_type iterator = leaf.leaves.begin(); iterator != leaf.leaves.end(); iterator++) 
	{
		vector<PListType>* pList = (*iterator).second->GetPList();
		archiveCollective->WriteArchiveMapMMAP(pList, (*iterator).first);
		
		delete iterator->second->GetPList();
		delete iterator->second;
	}
	leaf.ResetMap();

	archiveCollective->WriteArchiveMapMMAP(NULL, "", true);
	archiveCollective->CloseArchiveMMAP();

	archiveCollective->DumpPatternsToDisk(currLevel);

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
			stringstream builder;
			builder << "Chunks Failed to delete " << fileNameToBeRemoved << ": " << strerror(errno) << '\n';
			Logger::WriteLog(builder.str());
		}
		else
		{
			stringstream builder;
			builder << "Chunk succesfully deleted " << fileNameToBeRemoved << '\n';
			Logger::WriteLog(builder.str());
		}

		string fileNameToBeRemovedPatterns = folderLocation;
		fileNameToBeRemovedPatterns.append(fileNames[i].c_str());
		fileNameToBeRemovedPatterns.append("Patterns.txt");
	
		if( remove( fileNameToBeRemovedPatterns.c_str() ) != 0)
		{
			stringstream builder;
			builder << "Chunks Failed to delete " << fileNameToBeRemovedPatterns << ": " << strerror(errno) << '\n';
			Logger::WriteLog(builder.str());
			
		}
		else
		{
			stringstream builder;
			builder << "Chunk succesfully deleted " << fileNameToBeRemovedPatterns << '\n';
			Logger::WriteLog(builder.str());
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
			stringstream builder;
			builder << "Archives Failed to delete '" << fileNameToBeRemoved << "': " << strerror(errno) << '\n';
			Logger::WriteLog(builder.str());
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
		stringstream builder;
		builder << "Archive Failed to delete '" << fileNameToBeRemoved << "': " << strerror(errno) << '\n';
		Logger::WriteLog(builder.str());
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
		stringstream builder;
		builder << "Chunk Failed to delete " << fileNameToBeRemoved << ": " << strerror(errno) << '\n';
		Logger::WriteLog(builder.str());
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
		stringstream builder;
		builder << "Chunk Failed to delete '" << fileNameToBeRemovedPatterns << "': " << strerror(errno) << '\n';
		Logger::WriteLog(builder.str());
	}
	else
	{
		stringstream builder;
		builder << "Chunk succesfully deleted " << fileNameToBeRemovedPatterns << '\n';
		Logger::WriteLog(builder.str());
	}
	
}