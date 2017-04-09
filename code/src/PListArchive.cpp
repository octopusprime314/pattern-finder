#include "PListArchive.h"
#include "MemoryUtils.h"
#include <exception>
#include "Forest.h"
#include <algorithm>

PListType PListArchive::hdSectorSize;
PListType PListArchive::totalLoops;
PListType PListArchive::writeSize;

PListArchive::PListArchive(void)
{
}

PListArchive::PListArchive(string fileName, bool create)
{
	try
	{
		totalWritten = 0;
		mapper = NULL;
		prevFileIndex = 0;

		endOfFileReached = false;
		patternName = fileName;
		string file;
		
		file.append(LOGGERPATH);
		file.append(fileName);
		file.append(".txt");
		fd = -1;

		if(create)
		{
#if defined(_WIN64) || defined(_WIN32)
			fd = open(file.c_str(), O_RDWR | O_CREAT, _S_IREAD | _S_IWRITE);
#elif defined(__linux__)
			fd = open(file.c_str(), O_RDWR | O_CREAT, 0644);
#endif
			if(fd == -1)
			{
				stringstream streaming;
				streaming << "Why are we truncating the file " << file << endl;
				streaming << " and errno is "<< strerror(errno) << endl;
				Logger::WriteLog(streaming.str());
				cout << streaming.str() << endl;
				fd = open(file.c_str(), O_RDWR | O_TRUNC);
			}
		}
		else
		{
			fd = open(file.c_str(), O_RDONLY);
		}

		this->fileName = file;
		if(fd < 0)
		{
			
			stringstream stringbuilder;
			stringbuilder << file.c_str() << " file not found!";
			stringbuilder << " and errno is "<< strerror(errno) << endl;
			Logger::WriteLog(stringbuilder.str());
			endOfFileReached = true;
			return;
		}
		
		fileIndex = 0;
		startingIndex = 0;
		mappingIndex = 0;
		prevMappingIndex = 0;
	}
	catch(exception e)
	{
		string error("Exception occurred in method PListArchive Costructor -> ");
		error.append(e.what());
		Logger::WriteLog(error + "\n");
		cout << error << endl;
	}
}
PListArchive::~PListArchive(void)
{
}

void PListArchive::MappingError(int& fileDescriptor, string fileName)
{

#if defined(__linux__)
	DIR *dp;
   	struct dirent *ep;
	
	int pid = getpid();
	stringstream folder;
	folder << "/proc/" << pid << "/fd/";
   	dp = opendir (folder.str().c_str());
   	if (dp != NULL)
     	{
       		while (ep = readdir (dp))
         		cout << ep->d_name << endl;
       		(void) closedir (dp);
     	}
   	else
     		perror ("Couldn't open the directory");

	folder.str("");
	folder << "/proc/" << pid << "/map_files/";
	cout << folder.str() << endl;
	dp = opendir (folder.str().c_str());
	int counter = 0;
   	if (dp != NULL)
     	{
       		while (ep = readdir (dp))
			{
         		//cout << ep->d_name << endl;
				counter++;
			}
       		(void) closedir (dp);
     	}
   	else
     		perror ("Couldn't open the directory");
	cout << "Number of mapped files is: " << counter << endl;
#endif

	stringstream handle;
	handle << "Errno is "<< strerror(errno) << endl;
	handle << "error mapping the file " << fileName << endl;
	handle << "file descriptor: " << fileDescriptor << endl;
	handle << "end of file reached: " << endOfFileReached << endl;
	Logger::WriteLog(handle.str());
	cout << handle.str();
	close(fileDescriptor);

	fileDescriptor = -1;
	endOfFileReached = true;
}
	
void PListArchive::UnMappingError(int& fileDescriptor, string fileName)
{
	stringstream handle;
	handle << "Errno is "<< strerror(errno) << endl;
	handle << "error un-mapping the file " << fileName << endl;
	handle << "file descriptor: " << fileDescriptor << endl;
	handle << "end of file reached: " << endOfFileReached << endl;
	Logger::WriteLog(handle.str());
	cout << handle.str();
	close(fileDescriptor);

	fileDescriptor = -1;
	endOfFileReached = true;
}

void PListArchive::SeekingError(int& fileDescriptor, string fileName)
{
	close(fileDescriptor);
	fileDescriptor = -1;
	endOfFileReached = true;
	stringstream handle;
	handle << "error calling lseek() to 'stretch' the file " << fileName << endl;
	Logger::WriteLog(handle.str());
}

void PListArchive::ExtendingFileError(int& fileDescriptor, string fileName)
{
	close(fileDescriptor);
	fileDescriptor = -1;
	endOfFileReached = true;
	stringstream handle;
	handle << "error writing last byte of the file " << fileName << endl;
	Logger::WriteLog(handle.str());
}

void PListArchive::GetPListArchiveMMAP(vector<vector<PListType>*> &stuffedPListBuffer, double chunkSizeInMB)
{
	long long result;
	PListType *map;  /* mmapped array of char's */
	
	PListType pListGlobalIndex = -1;
	PListType listCount = 0;

	PListType globalTotalMemoryInBytes = 0;
	
	PListType totalCount = 0;
	try
	{
		bool finishedFlag = false;
		bool trimPList = false;

		while(!finishedFlag)
		{
			
			map = (PListType *)mmap64 (0, hdSectorSize, PROT_READ, MAP_SHARED, fd, fileIndex);

			if (map == MAP_FAILED) 
			{
				MappingError(fd, this->fileName);
				return;
			}
		
			PListType PListBuffSize = hdSectorSize/sizeof(PListType);
			/* Now do something with the information. */
			PListType i;
			
			for (i = startingIndex; i < PListBuffSize; i++) 
			{
				if(listCount == 0)
				{
					if(pListGlobalIndex != -1)
					{
						stuffedPListBuffer[pListGlobalIndex]->shrink_to_fit();

						if(chunkSizeInMB != 0)
						{
							//size of vector container
							globalTotalMemoryInBytes += 32;
							//Size of total vector on the heap
							globalTotalMemoryInBytes += stuffedPListBuffer[pListGlobalIndex]->capacity()*sizeof(PListType);

							if(((globalTotalMemoryInBytes + (stuffedPListBuffer.capacity()*24))/1000000.0f) >= chunkSizeInMB)
							{
								finishedFlag = true;
										
								if(stuffedPListBuffer.size() > 1)
								{
									trimPList = true;

									fileIndex = prevListIndex;
									startingIndex = prevStartingIndex;

									delete stuffedPListBuffer[pListGlobalIndex];
									stuffedPListBuffer.pop_back();
								}
								break;
							}
						}
					}

					prevStartingIndex = i;
					prevListIndex = fileIndex;

					listCount = (PListType) map[i];

					//If listCount equals zero then we are at the end of the pList data stream bam bitches
					if(listCount == 0)
					{
						finishedFlag = true;
						endOfFileReached = true;
						break;
					}
					stuffedPListBuffer.push_back(new vector<PListType>());
						
					pListGlobalIndex++;
					
					stuffedPListBuffer[pListGlobalIndex]->reserve(listCount);
					
				}
				else
				{
					stuffedPListBuffer[pListGlobalIndex]->push_back(map[i]);
					listCount--;
				}
			}

			if(!trimPList)
			{
				if(i == PListBuffSize)
				{
					startingIndex = 0;
					fileIndex += hdSectorSize;
				}
				else
				{
					startingIndex = i;
				}
			}
			if (munmap(map, PListBuffSize) == -1) 
			{
				UnMappingError(fd, this->fileName);
				return;
			}
		}
	}
	catch(exception e)
	{
		string error("Exception occurred in method GetPListArchiveMMAP -> ");
		error.append(e.what());
		Logger::WriteLog(error + "\n");
		cout << error << endl;
	}
}

bool PListArchive::IsEndOfFile()
{
	return endOfFileReached;
}

bool PListArchive::Exists()
{
	if(fd > 0)
	{
		return true;
	}
	else
	{
		return false;
	}
}

void PListArchive::WriteArchiveMapMMAP(const vector<PListType> &pListVector, const PatternType &pattern, bool flush, bool forceClose)
{
	try
	{
		if(flush)
		{
			//Kick off thread that flushes cached memory mapping to disk asynchronously and it may be bad 
			if(mapper != NULL)
			{
				msync(mapper, hdSectorSize, MS_ASYNC);
				//Deallocate only when it has been completely used
				if (munmap(mapper, hdSectorSize) == -1) 
				{
					UnMappingError(fd, this->fileName);
					return;
				}
				mapper = NULL;
			}
			
			totalWritten = 0;
			
			return;
		}
		long long result;
		if(pListVector.size() > 0)
		{
			if(pattern.size() > 0)
			{
				stringBuffer.push_back(pattern);
			}
			mappingIndex += ((pListVector.size() + 1)*sizeof(PListType));
		}

		PListType startPoint = ((prevMappingIndex/sizeof(PListType)) % totalLoops);
		PListType tempMapIndex = mappingIndex;
		mappingIndex = prevMappingIndex;
		prevMappingIndex = tempMapIndex;
		
		
		fileIndex = (mappingIndex/hdSectorSize)*hdSectorSize;

		bool done = false;

		//overshoot how much we are going to write just in case by adding one extra to the offset
		// i don't know why but it makes it work because I believe before not everything was being written or something was getting overwritten
		PListType offset = ceil(((double)(pListVector.size()*sizeof(PListType)))/((double)hdSectorSize)) + 1;

		//If offset is less than disk write size then write whatever can be done
		if(offset <= 0)
		{
			offset = 1;
		}

		PListType pListSize = pListVector.size();

		PListType offsetStep = 0;

		bool grabbedCount = false;

	#if defined(__linux__)
		if((fileIndex + (offset*hdSectorSize) - 1) >= prevFileIndex)
		{
			result = lseek64(fd, fileIndex + (offset*hdSectorSize) - 1, SEEK_SET);
			size_t writeSize = write(fd, "", 1);
			prevFileIndex = fileIndex + (offset*hdSectorSize) - 1;
		}
	#endif

		int i;
		for(i = 0; i < offset && !done && pListSize > 0; i++)
		{
			if(startPoint == 0 || mapper == NULL)
			{
				if(mapper != NULL)
				{
					msync(mapper, hdSectorSize, MS_ASYNC);
					//Deallocate only when it has been completely used
					if (munmap(mapper, hdSectorSize) == -1) 
					{
						UnMappingError(fd, this->fileName);
						return;
					}
					mapper = NULL;
				}

				mapper = (PListType *)mmap64(0, hdSectorSize, PROT_WRITE, MAP_SHARED, fd, fileIndex);
				
				if (mapper == MAP_FAILED) 
				{
					MappingError(fd, this->fileName);
					return;
				}
			}
			/* Now write unsigned longs's to the file as if it were memory (an array of longs).
				*/

			PListType listVectorSize = pListVector.size();
			int z;
			for (z = startPoint; z < totalLoops && offsetStep < listVectorSize; ++z) 
			{
				if(!grabbedCount)
				{
					grabbedCount = true;
					mapper[z] = pListVector.size();
				}
				else
				{
					mapper[z] = pListVector[offsetStep];
					offsetStep++;
				}
				totalWritten++;

				if(offsetStep >= listVectorSize)
				{
					done = true;
					break;
				}
			}


			//No longer need to use a starting point after first write hehe :)
			startPoint = 0;

			if(z == totalLoops)
			{
				fileIndex += hdSectorSize;
			}

		}

		
		mappingIndex = prevMappingIndex;

	}
	catch(exception e)
	{
		string error("Exception occurred in method WriteArchiveMapMMAP -> ");
		error.append(e.what());
		Logger::WriteLog(error + "\n");
		cout << error << endl;
	}

}

vector<string>* PListArchive::GetPatterns(unsigned int level, PListType count)
{
	if(fd == -1)
	{
		return NULL;
	}

	PListType preservedFileIndex = fileIndex;

	long long result;
	char *mapChar;  /* mmapped array of char's */

	//Add plus two to account for patternLength and mapEntries.
	PListType sizeToReadForPatterns = count*level;
	PListType totalReadsForPatterns = ceil(((double)(sizeToReadForPatterns))/((double)hdSectorSize));
	if(totalReadsForPatterns == 0)
	{
		totalReadsForPatterns++;
	}
	vector<string> *newStringBuffer = NULL;
	PListType newStringBufferIndex = 0;
	PListType offstep = 0;
	unsigned int prevIndexForChar = 0;

	PListType sizeToRead = 0;
	if(count == 0)
	{
		return NULL;
	}
	else
	{
		newStringBuffer = new vector<string>(count);
	}

	string completePattern( level, ' ');

	try
	{
		for(PListType piss = 0; piss < totalReadsForPatterns; piss++)
		{

	#if defined(_WIN64) || defined(_WIN32)
			result = _lseeki64(fd, fileIndex, SEEK_SET);
	#elif defined(__linux__)
			result = lseek64(fd, fileIndex, SEEK_SET);
	#endif
			mapChar = (char *)mmap64 (0, hdSectorSize, PROT_READ, MAP_SHARED, fd, fileIndex);

			if (mapChar == MAP_FAILED) 
			{
				MappingError(fd, this->fileName);
				return NULL;
			}
		
			PListType PListBuffSize = hdSectorSize/sizeof(char);
			/* Now do something with the information. */

			if(piss < totalReadsForPatterns)
			{
				PListBuffSize = hdSectorSize/sizeof(char);

				bool endWhistle = false;
				PListType i = 0;
				while (i < PListBuffSize && offstep < count) 
				{
					for(PListType charIt = prevIndexForChar; charIt < level; charIt++)
					{
						if(i >= PListBuffSize)
						{
							prevIndexForChar = charIt;
							endWhistle = true;
							break;
						}
						else
						{
							prevIndexForChar = 0;
						}
					
						if(!endWhistle)
						{
							completePattern[charIt] = mapChar[i];
						}
						i++;
					}
					if(!endWhistle)
					{ 
						(*newStringBuffer)[newStringBufferIndex++] = completePattern;
						offstep++;
					}
				}

				if (munmap(mapChar, PListBuffSize) == -1) 
				{
					UnMappingError(fd, this->fileName);
					return NULL;
				}
			}
		
			fileIndex += hdSectorSize;
		}
		fileIndex = preservedFileIndex;
		newStringBuffer->shrink_to_fit();
	}
	catch(exception e)
	{
		string error("Exception occurred in method GetPatterns -> ");
		error.append(e.what());
		Logger::WriteLog(error + "\n");
		cout << error << endl;
	}
	return newStringBuffer;
}

void PListArchive::DumpPatternsToDisk(unsigned int level)
{
	try
	{
		long long result;
		
		char *mapForChars = NULL;  /* mmapped array of char's */

		string file;
	
		file.append(LOGGERPATH);
		std::string::size_type i = fileName.find(".txt");
		string tempString = fileName;
		tempString.erase(i, 4);
		stringstream temp;
		temp << tempString;
		temp << "Patterns";
		temp << ".txt";
		file = temp.str();

	#if defined(_WIN64) || defined(_WIN32)
		int mapFD = open(file.c_str(), O_RDWR | O_CREAT, _S_IREAD | _S_IWRITE);
	#elif defined(__linux__)
		int mapFD = open(file.c_str(), O_RDWR | O_CREAT, 0644);
	#endif

		if(mapFD < 0)
		{
			stringstream stringbuilder;
			stringbuilder << file.c_str() << " map pattern file not found!";
			stringbuilder << " and errno is "<< strerror(errno) << endl;
			Logger::WriteLog(stringbuilder.str());
			endOfFileReached = true;
			return;
		}



		bool done = false;

		//overshoot how much we are going to write just in case by adding one extra to the offset
		// i don't know why but it makes it work because I believe before not everything was being written or something was getting overwritten
		PListType totalWritesForCharTypes = ceil(((double)(stringBuffer.size()*level))/((double)hdSectorSize)) + 1;

		//If offset is less than disk write size then write whatever can be done
		if(totalWritesForCharTypes == 0 )
		{
			totalWritesForCharTypes = 1;
		}

		PListType finalWriteSize = (totalWritesForCharTypes)*hdSectorSize; 

		//Reset final Index
		fileIndex = 0;

		bool firstWritePatternLength = false;

		unsigned int prevIndexForChar = 0;
		PListType prevIndexForString = 0;
		bool unfinished = false;
		PListType stringIndex = 0;

	#if defined(__linux__)
		result = lseek64(mapFD, fileIndex + finalWriteSize - 1, SEEK_SET);
		size_t writeSize = write(mapFD, "", 1);
	#endif
	
		
		for(int i = 0; i < totalWritesForCharTypes && !done; i++)
		{
			mapForChars = (char *)mmap64(0, hdSectorSize, PROT_WRITE, MAP_SHARED, mapFD, fileIndex);

			if (mapForChars == MAP_FAILED) 
			{
				stringstream uhoh;
				uhoh << "fileIndex : " << fileIndex << endl;
				Logger::WriteLog(uhoh.str());
				MappingError(mapFD, file);
				return;
			}

			stringIndex = prevIndexForString;

			unsigned int z = 0;
			
			while(stringIndex < stringBuffer.size() && z < (hdSectorSize/sizeof(char)) && !done)
			{
				int patternIt = prevIndexForChar;
				while(patternIt < level && z < (hdSectorSize/sizeof(char)) )
				{
					mapForChars[z] = stringBuffer[stringIndex][patternIt];
					z++;
					patternIt++;
				}
				
				if(patternIt == level)
				{
					prevIndexForChar = 0;
					stringIndex++;
				}
				else
				{
					prevIndexForChar = patternIt;
				}
				
				prevIndexForString = stringIndex;
				
				if(stringIndex == stringBuffer.size())
				{
					done = true;
				}
			}	

			fileIndex += hdSectorSize;
		
			/* Don't forget to free the mmapped memory
				*/
			msync(mapForChars, hdSectorSize, MS_ASYNC);

			if (munmap(mapForChars, hdSectorSize) == -1) 
			{
				UnMappingError(mapFD, file);
				return;
			}
		}
		
		close(mapFD);
	}
	catch(exception e)
	{
		
		string error("Exception occurred in method DumpPatternsToDisk -> ");
		error.append(e.what());
		Logger::WriteLog(error + "\n");
		cout << error << endl;
	}
}


//Close log and save time
void PListArchive::CloseArchiveMMAP()
{
	try
	{
		/* Un-mmaping doesn't close the file, so we still need to do that.
		 */
		if(fd != -1)
		{
			close(fd);
		}
	}
	catch(exception e)
	{
		
		string error("Exception occurred in method CloseArchiveMMAP -> ");
		error.append(e.what());
		Logger::WriteLog(error + "\n");
		cout << error << endl;
	}
}

