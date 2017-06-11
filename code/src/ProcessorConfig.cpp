#include "ProcessorConfig.h"
#include <locale>
#if defined(_WIN64) || defined(_WIN32)
#include "Dirent.h"
#elif defined(__linux__)
#include <dirent.h>
#endif


ConfigurationParams ProcessorConfig::config;

ProcessorConfig::ProcessorConfig(void)
{
}

ProcessorConfig::~ProcessorConfig(void)
{
}

ConfigurationParams ProcessorConfig::GetConfig(int argc, char **argv)
{
	config.findBestThreadNumber = false;
	config.usingMemoryBandwidth = false;
	config.memoryBandwidthMB = 0;
	config.levelToOutput = 0;
	config.usingPureRAM = false;
	config.usingPureHD = false;
	//Default pattern occurence size to 2
	config.minOccurrence = 2;
	config.nonOverlappingPatternSearch = ANY_PATTERNS;
	config.history = 0;
	config.threadLimitation = 0;
	config.lowRange = config.highRange = 0;

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
			config.minimum = atoi(argv[i + 1]);
			minEnter = true;
			i++;
		}
		else if (arg.compare("-max") == 0)
		{
			// We know the next argument *should* be the maximum pattern to display
			config.maximum = atoi(argv[i + 1]);
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
				
				if(fileTest.find(':') != string::npos || fileTest[0] == '/')
				{
					tempFileName = argv[i + 1];
				}
				config.files.push_back(new FileReader(tempFileName, isFile));
				config.fileSizes.push_back(config.files.back()->fileStringSize);
				i++;
			}
			else if(fileTest.find('.') == string::npos && fileTest[0] != '-')
			{
			#if defined(_WIN64) || defined(_WIN32)
				header = "../../Database/";
			#elif defined(__linux__)
				header = "../Database/";
			#endif
				header.append(fileTest);
				header.append("/");

				//Access files with full path
				if(fileTest.find(":") != std::string::npos || fileTest[0] == '/')
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
			Logger::verbosity = atoi(argv[i + 1]);
			i++;
		}
		else if (arg.compare("-c") == 0)
		{
			config.findBestThreadNumber = true;
		}
		else if (arg.compare("-threads") == 0)
		{
			// We know the next argument *should* be the maximum pattern to display
			config.numThreads = atoi(argv[i + 1]);
			threadsEnter = true;
			i++;
		}
		else if(arg.compare("-mem") == 0)
		{
			config.memoryBandwidthMB = atoi(argv[i + 1]);
			config.usingMemoryBandwidth = true;
			i++;
		}
		else if(arg.compare("-lr") == 0)
		{
			config.lowRange = atoi(argv[i + 1]);
			i++;
		}
		else if(arg.compare("-hr") == 0)
		{
			config.highRange = atoi(argv[i + 1]);
			i++;
		}
		else if(arg.compare("-n") == 0)
		{
			config.nonOverlappingPatternSearch = NONOVERLAP_PATTERNS;
		}
		else if(arg.compare("-o") == 0)
		{
			config.nonOverlappingPatternSearch = OVERLAP_PATTERNS;
		}
		else if(arg.compare("-lev") == 0 )
		{
			config.levelToOutput = atoi(argv[i + 1]);
			i++;
		}
		else if(arg.compare("-his") == 0)
		{
			config.history = atoi(argv[i + 1]);
			i++;
		}
		else if(arg.compare("-ram") == 0)
		{
			config.usingPureRAM = true;
		}
		else if(arg.compare("-hd") == 0)
		{
			config.usingPureHD = true;
		}
		else if(arg.compare("-p") == 0)
		{
			config.patternToSearchFor = argv[i+1];
			config.maximum = static_cast<PListType>(config.patternToSearchFor.size());
			i++;
		}
		else if(arg.compare("-i") == 0)
		{
			config.minOccurrence = atoi(argv[i+1]);
			i++;
		}
		else if(arg.compare("-cov") == 0)
		{
			coverageTracking = true;
			i++;
		}
		else if(arg.compare("-l") == 0)
		{
			config.threadLimitation = atoi(argv[i+1]);
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

	//Make maximum the largest if not entered
	if(!maxEnter)
	{
		config.maximum = -1;
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

	//If min not specified then make the smallest pattern of 0
	if (!minEnter)
	{
		config.minimum = 0;
	}
	//If numCores is not specified then we use number of threads supported cores plus the main thread
	if (!threadsEnter /*|| numThreads > concurentThreadsSupported*/)
	{
		config.numThreads = concurentThreadsSupported;
	}

	int bestThreadCount = 0;
	double fastestTime = 1000000000.0f;
	config.testIterations = 0;
	if (config.findBestThreadNumber)
	{
		config.numThreads = 1;
		if(config.threadLimitation != 0)
		{
			config.testIterations = config.threadLimitation;
		}
		else
		{
			config.testIterations = concurentThreadsSupported;
		}
	}
	return config;
}

void ProcessorConfig::FindFiles(string directory)
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
						config.files.push_back(file);
						config.fileSizes.push_back(config.files.back()->fileStringSize);
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
				config.files.push_back(file);
				config.fileSizes.push_back(config.files.back()->fileStringSize);
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

void ProcessorConfig::DisplayHelpMessage()
{
	bool isFile;
	FileReader tempHelpFile(HELPFILEPATH, isFile, true);
	tempHelpFile.LoadFile();
	cout << tempHelpFile.fileString << endl;
}
