#include "PListArchive.h"
#include "MemoryUtils.h"
#include <exception>

PListArchive::PListArchive(void)
{
}

PListArchive::PListArchive(string fileName, bool IsPList, bool isBackup)
{
	try
	{
		string file;
		if(IsPList && !isBackup)
		{
			file.append("../Log/");
			file.append(fileName);
			file.append(".txt");
		}
		else if(IsPList && isBackup)
		{
			file.append("../Log/BackupLog/");
			file.append(fileName);
			file.append(".txt");
		}
		else
		{
			file.append(fileName);
		}
		fd = open(file.c_str(), O_RDWR );
		if(fd < 0)
		{
			cout << file.c_str() << " file not found!" << endl;
			return;
		}
		this->fileName = file;

		fileSize = -1;
		fileIndex = 0;
		hdSectorSize = 2097152;
		startingIndex = 0;
		mappingIndex = 0;
		prevMappingIndex = 0;
	}
	catch(exception e)
	{
		cout << "Exception occurred in method PListArchive constructor -> " << e.what() << endl;
	}
}
PListArchive::~PListArchive(void)
{
}

unsigned long long PListArchive::GetFileChunkSize(PListType chunkSizeInBytes)
{
	PListType sizeToRead = 0;
	try
	{	
		//If chunk is larger than file
		//If file size hasn't been found yet grab it once
		if(fileSize == -1)
		{
			fileSize = MemoryUtils::FileSize(this->fileName);
		}
		if(chunkSizeInBytes > fileSize)
		{
			chunkSizeInBytes = fileSize;
		}

		unsigned long long chunksToBeReading = chunkSizeInBytes/hdSectorSize;
		if(chunkSizeInBytes%hdSectorSize != 0)
		{
			chunksToBeReading++;
		}
		sizeToRead = chunksToBeReading*hdSectorSize;
	}
	catch(exception e)
	{
		cout << "Exception occurred in method GetFileChunkSize -> " << e.what() << endl;
	}
	return sizeToRead;
}

string PListArchive::GetFileChunk(PListType index, PListType chunkSizeInBytes)
{
	char *map;  /* mmapped array of char's */
	string fileString;
	try
	{
		double InitMemoryMB = MemoryUtils::GetProgramMemoryConsumption();
	
		//If file size hasn't been found yet grab it once
		if(fileSize == -1)
		{
			fileSize = MemoryUtils::FileSize(this->fileName);
		}
	
		PListType chunkIndex = (index/hdSectorSize)*hdSectorSize;
		if(index%hdSectorSize != 0)
		{
			chunkIndex += hdSectorSize;
		}
		unsigned long long chunksToBeReading = chunkSizeInBytes/hdSectorSize;
		if(chunkSizeInBytes%hdSectorSize != 0)
		{
			chunksToBeReading++;
		}
		PListType sizeToRead = chunksToBeReading*hdSectorSize;
		if(sizeToRead > fileSize - chunkIndex)
		{
			sizeToRead = fileSize - chunkIndex;
		}
		PListType totalReads = sizeToRead/hdSectorSize;
		if(sizeToRead%hdSectorSize != 0)
		{
			totalReads++;
		}

		fileString.clear();
		fileString.resize(sizeToRead);  
		PListType chunkSizePerRead = hdSectorSize;

		for(PListType piss = 0; piss < totalReads; piss++)
		{
			if((sizeToRead - (piss*hdSectorSize)) < hdSectorSize)
			{
				chunkSizePerRead = ((sizeToRead - (piss*hdSectorSize)))/sizeof(char);
			}
	
			map = (char *)mmap64 (0, chunkSizePerRead, PROT_READ, MAP_SHARED, fd, chunkIndex);
	
			if (map == MAP_FAILED) 
			{
				close(fd);
				perror("Error mmapping the file");
			}
		
			unsigned long long PListBuffSize = hdSectorSize/sizeof(char);

			if((sizeToRead - (piss*hdSectorSize)) < hdSectorSize)
			{
				PListBuffSize = ((sizeToRead - (piss*hdSectorSize)))/sizeof(char);
			}
			
			for (unsigned long long i = 0; i < PListBuffSize; i++) 
			{
				fileString[(piss*hdSectorSize) + i] = (char) map[i];
			}

			if (munmap(map, PListBuffSize) == -1) 
			{
				perror("Error un-mmapping the file");
			}
		
			chunkIndex += hdSectorSize;
		}
	}
	catch(exception e)
	{
		cout << "Exception occurred in method GetFileChunk -> " << e.what() << endl;
	}
	return fileString; 
}

vector<vector<PListType>*>* PListArchive::GetPListArchiveMMAP(PListType chunkSizeInMB)
{
	long long result;
	PListType *map;  /* mmapped array of char's */

	double InitMemoryMB = MemoryUtils::GetProgramMemoryConsumption();
	
	//If file size hasn't been found yet grab it once
	if(fileSize == -1)
	{
		fileSize = MemoryUtils::FileSize(this->fileName);
	}

	PListType sizeToRead = 0;
	if(chunkSizeInMB == 0)
	{
		sizeToRead = fileSize;
	}
	else
	{
		sizeToRead = chunkSizeInMB*1000000;
	}

	if(sizeToRead > fileSize - fileIndex)
	{
		sizeToRead = fileSize - fileIndex;
	}
	PListType totalReads = ((double)sizeToRead)/((double)hdSectorSize);
	PListType pListGlobalIndex = -1;
	PListType listCount = 0;

	if(totalReads < 1 && fileSize > 0)
	{
		totalReads = 1;
	}
	else if(totalReads < 1 && fileSize == 0)
	{
		totalReads = 0;
	}
	
	vector<vector<PListType>*>* stuffedPListBuffer = new vector<vector<PListType>*>();
	
	try
	{
		bool finishedFlag = false;

		for(PListType piss = 0; piss < totalReads && !finishedFlag; piss++)
		{

	#if defined(_WIN64) || defined(_WIN32)
			result = _lseeki64(fd, fileIndex, SEEK_SET);
	#elif defined(__linux__)
			result = lseek64(fd, fileIndex, SEEK_SET);
	#endif
			map = (PListType *)mmap64 (0, hdSectorSize, PROT_READ, MAP_SHARED, fd, fileIndex);
	
			if (map == MAP_FAILED) 
			{
				close(fd);
				perror("Error mmapping the file");
			}
		
			PListType PListBuffSize = hdSectorSize/sizeof(PListType);
			/* Now do something with the information. */

			if(piss < totalReads)
			{
				PListBuffSize = hdSectorSize/sizeof(PListType);

				if((sizeToRead - (piss*hdSectorSize)) < hdSectorSize)
				{
					PListBuffSize = ((sizeToRead - (piss*hdSectorSize)))/sizeof(PListType);
				}
				if(startingIndex != 0)
				{
					PListBuffSize = hdSectorSize/sizeof(PListType) - startingIndex;
				}
			
				for (PListType i = startingIndex; i < PListBuffSize + startingIndex; i++) 
				{
					if(listCount == 0)
					{
						if(pListGlobalIndex != -1)
						{
							(*stuffedPListBuffer)[pListGlobalIndex]->shrink_to_fit();

							if(chunkSizeInMB != 0)
							{
								double CurrentMemoryMB = MemoryUtils::GetProgramMemoryConsumption() - InitMemoryMB;
								if(CurrentMemoryMB >= chunkSizeInMB)
								{
									finishedFlag = true;
									fileIndex = prevListIndex;
									startingIndex = prevStartingIndex;
									delete (*stuffedPListBuffer)[pListGlobalIndex];
									stuffedPListBuffer->pop_back();
									cout << "Didn't make it to the end of file: " << fileName << endl;
									break;
								}
							}
						}

						listCount = (PListType) map[i];
						//cout << "List count: " << listCount << endl;
						prevListIndex = fileIndex;
						prevStartingIndex = i;

						//If listCount equals zero then we are at the end of the pList data stream bam bitches
						if(listCount == 0)
						{
							break;
						}
						stuffedPListBuffer->push_back(new vector<PListType>());
						pListGlobalIndex++;
					}
					else
					{
						(*stuffedPListBuffer)[pListGlobalIndex]->push_back(map[i]);
						listCount--;
					}
				}

				if(startingIndex != 0 && !finishedFlag)
				{
					startingIndex = 0;
				}

				//If pattern is not done we have to resize again
				if(listCount > 0 && piss == totalReads - 1 && !finishedFlag)
				{
					if(chunkSizeInMB != 0)
					{
						finishedFlag = true;
						fileIndex = prevListIndex;
						startingIndex = prevStartingIndex;
						delete (*stuffedPListBuffer)[pListGlobalIndex];
						stuffedPListBuffer->pop_back();
						cout << "Didn't make it to the end of file: " << fileName << endl;
					}
				}
				//if size of chunk is less than hdSector size
				else if(sizeToRead <= hdSectorSize)
				{
					fileIndex += hdSectorSize;
				}
				if (munmap(map, PListBuffSize) == -1) 
				{
					perror("Error un-mmapping the file");
				}
			}
		
			if(!finishedFlag)
			{
				fileIndex += hdSectorSize;
			}
		
		}
	}
	catch(exception e)
	{
		cout << "Exception occurred in method GetPListArchiveMMAP -> " << e.what() << endl;
	}
	return stuffedPListBuffer;
}

bool PListArchive::IsEndOfFile()
{
	//If file size hasn't been found yet grab it once
	if(fileSize == -1)
	{
		fileSize = MemoryUtils::FileSize(this->fileName);
	}

	//if fileIndex is larger than the file size we are done here.
	if(fileIndex >= fileSize - 1 || fileSize == 0)
	{
		return true;
	}
	else
	{
		return false;
	}
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

void PListArchive::DumpContents()
{
	try
	{
		if(fd != -1)
		{
			close(fd);
			fd =  open(fileName.c_str(), O_RDWR | O_TRUNC);
		}
	}
	catch(exception e)
	{
		cout << "Exception occurred in method DumpContents -> " << e.what() << endl;
	}
}

void PListArchive::WriteArchiveMapMMAP(vector<PListType> *pListVector, string pattern, bool flush)
{
	try
	{
		long long result;
		PListType *map = NULL;  /* mmapped array of char's */
		if(pListVector != NULL)
		{
			if(pattern.size() > 0)
			{
				stringBuffer.push_back(pattern);
			}
			mappingIndex += ((pListVector->size() + 1)*sizeof(PListType));
			pListBuffer.push_back(pListVector->size());
			pListBuffer.insert(pListBuffer.end(), pListVector->begin(), pListVector->end());
		}

		if(pListBuffer.size()*sizeof(PListType) > hdSectorSize || (flush && pListBuffer.size() > 0))
		{
			PListType startPoint = (prevMappingIndex % hdSectorSize)/sizeof(PListType);
			PListType tempMapIndex = mappingIndex;
			mappingIndex = prevMappingIndex;
			prevMappingIndex = tempMapIndex;
		
			fileIndex = (mappingIndex/hdSectorSize)*hdSectorSize;

			bool doneWithThisShit = false;

			//overshoot how much we are going to write just in case by adding one extra to the offset
			// i don't know why but it makes it work because I believe before not everything was being written or something was getting overwritten
			// who fucking knows but baby does it work now!
			PListType offset = ceil(((double)(pListBuffer.size()*sizeof(PListType)))/((double)hdSectorSize)) + 1;

			//If offset is less than disk write size then write whatever can be done
			if(offset <= 0)
			{
				offset = 1;
			}

			/* stretch the file size to the size of the (mmapped) array of unsigned char
				*/
		
		#if defined(_WIN64) || defined(_WIN32)
			//cout << "file id: " << fd << " write position: " << fileIndex + (offset*hdSectorSize) - 1 << endl;
			result = _lseeki64(fd, fileIndex + (offset*hdSectorSize) - 1, SEEK_SET);
		#elif defined(__linux__)
			result = lseek64(fd, fileIndex + (offset*hdSectorSize) - 1, SEEK_SET);
		#endif
		
			if (result == -1) 
			{
				close(fd);
				perror("error calling lseek() to 'stretch' the file");
				exit(0);
			}
    
			/* something needs to be written at the end of the file to
				* have the file actually have the new size.
				* just writing an empty string at the current file position will do.
				*
				* note:
				*  - the current position in the file is at the end of the stretched 
				*    file due to the call to lseek().
				*  - an empty string is actually a single '\0' character, so a zero-byte
				*    will be written at the last byte of the file.
				*/
			result = write(fd, "", 1);
			if (result != 1) 
			{
				close(fd);
				perror("error writing last byte of the file");
				exit(0);
			}

			/* Now the file is ready to be mmapped.
				*/
			PListType offsetStep = 0;
		
			PListType pListSize = pListBuffer.size();

			for(int i = 0; i < offset && !doneWithThisShit; i++)
			{

				map = (PListType *)mmap64(0, hdSectorSize, PROT_WRITE, MAP_SHARED, fd, fileIndex);

				if (map == MAP_FAILED) 
				{
					close(fd);
					perror("Error mmapping the file");
					exit(0);
				}
    
				/* Now write unsigned longs's to the file as if it were memory (an array of longs).
					*/
				for (int z = startPoint; z < (hdSectorSize/sizeof(PListType)) && offsetStep < (offset*hdSectorSize/sizeof(PListType)); ++z) 
				{
					map[z] = pListBuffer[offsetStep];
					offsetStep++;

					if(offsetStep >= pListBuffer.size())
					{
						doneWithThisShit = true;
						break;
					}
				}

				//No longer need to use a starting point after first write hehe :)
				startPoint = 0;

				//Flush pages to hard disk
				msync(map, hdSectorSize, MS_SYNC);

				fileIndex += hdSectorSize;
		
				/* Don't forget to free the mmapped memory
					*/

				if (munmap(map, hdSectorSize) == -1) 
				{
					perror("Error un-mmapping the file");
					/* Decide here whether to close(fd) and exit() or not. Depends... */
					close(fd);
					exit(0);
				}
			}

			vector<PListType>::const_iterator first = pListBuffer.begin() + offsetStep;
			vector<PListType>::const_iterator last = pListBuffer.end();
			vector<PListType> tempPListBuffer(first, last);
			//If written clear the pList out
			pListBuffer.clear();
			//append rest of buffer if not empty
			if(!tempPListBuffer.empty())
			{
				pListBuffer.insert(pListBuffer.end(), tempPListBuffer.begin(), tempPListBuffer.end());
				tempPListBuffer.clear();
			}
			mappingIndex = prevMappingIndex;
		}
		if(flush)
		{
			pListBuffer.shrink_to_fit();
		}
	}
	catch(exception e)
	{
		cout << "Exception occurred in method WriteArchiveMapMMAP -> " << e.what() << endl;
		cout << "Vector size man! " << pListVector->size() << endl;
	}
}

void PListArchive::OpenArchiveMMAP()
{
	try
	{
		fd =  open(fileName.c_str(), O_RDWR );
	}
	catch(exception e)
	{
		cout << "Exception occurred in method OpenArchiveMMAP -> " << e.what() << endl;
	}
}

void PListArchive::WriteArchiveMapMMAPMeta(vector<PListType> *pListVector, string pattern, bool flush)
{
	try
	{
		long long result;
		PListType *map = NULL;  /* mmapped array of char's */
		if(pListVector != NULL)
		{
			if(pattern.size() > 0)
			{
				pListMetaData[pattern] = mappingIndex;
				stringBuffer.push_back(pattern);
			}
			mappingIndex += ((pListVector->size() + 1)*sizeof(PListType));
			pListBuffer.push_back(pListVector->size());
			pListBuffer.insert(pListBuffer.end(), pListVector->begin(), pListVector->end());
		}

		if(pListBuffer.size()*sizeof(PListType) > hdSectorSize || (flush && pListBuffer.size() > 0))
		{
			PListType startPoint = (prevMappingIndex % hdSectorSize)/sizeof(PListType);
			PListType tempMapIndex = mappingIndex;
			mappingIndex = prevMappingIndex;
			prevMappingIndex = tempMapIndex;
		
			fileIndex = (mappingIndex/hdSectorSize)*hdSectorSize;

			bool doneWithThisShit = false;

			//overshoot how much we are going to write just in case by adding one extra to the offset
			// i don't know why but it makes it work because I believe before not everything was being written or something was getting overwritten
			// who fucking knows but baby does it work now!
			PListType offset = ceil(((double)(pListBuffer.size()*sizeof(PListType)))/((double)hdSectorSize)) + 1;

			//If offset is less than disk write size then write whatever can be done
			if(offset <= 0)
			{
				offset = 1;
			}

			/* stretch the file size to the size of the (mmapped) array of unsigned char
				*/
		
		#if defined(_WIN64) || defined(_WIN32)
			//cout << "file id: " << fd << " write position: " << fileIndex + (offset*hdSectorSize) - 1 << endl;
			result = _lseeki64(fd, fileIndex + (offset*hdSectorSize) - 1, SEEK_SET);
		#elif defined(__linux__)
			result = lseek64(fd, fileIndex + (offset*hdSectorSize) - 1, SEEK_SET);
		#endif
		
			if (result == -1) 
			{
				close(fd);
				perror("error calling lseek() to 'stretch' the file");
				exit(0);
			}
    
			/* something needs to be written at the end of the file to
				* have the file actually have the new size.
				* just writing an empty string at the current file position will do.
				*
				* note:
				*  - the current position in the file is at the end of the stretched 
				*    file due to the call to lseek().
				*  - an empty string is actually a single '\0' character, so a zero-byte
				*    will be written at the last byte of the file.
				*/
			result = write(fd, "", 1);
			if (result != 1) 
			{
				close(fd);
				perror("error writing last byte of the file");
				exit(0);
			}

			/* Now the file is ready to be mmapped.
				*/
			PListType offsetStep = 0;
		
			PListType pListSize = pListBuffer.size();

			for(int i = 0; i < offset && !doneWithThisShit; i++)
			{

				map = (PListType *)mmap64(0, hdSectorSize, PROT_WRITE, MAP_SHARED, fd, fileIndex);

				if (map == MAP_FAILED) 
				{
					close(fd);
					perror("Error mmapping the file");
					exit(0);
				}
    
				/* Now write unsigned longs's to the file as if it were memory (an array of longs).
					*/
				for (int z = startPoint; z < (hdSectorSize/sizeof(PListType)) && offsetStep < (offset*hdSectorSize/sizeof(PListType)); ++z) 
				{
					map[z] = pListBuffer[offsetStep];
					offsetStep++;

					if(offsetStep >= pListBuffer.size())
					{
						doneWithThisShit = true;
						break;
					}
				}

				//No longer need to use a starting point after first write hehe :)
				startPoint = 0;

				//Flush pages to hard disk
				msync(map, hdSectorSize, MS_SYNC);

				fileIndex += hdSectorSize;
		
				/* Don't forget to free the mmapped memory
					*/

				if (munmap(map, hdSectorSize) == -1) 
				{
					perror("Error un-mmapping the file");
					/* Decide here whether to close(fd) and exit() or not. Depends... */
					close(fd);
					exit(0);
				}
			}

			vector<PListType>::const_iterator first = pListBuffer.begin() + offsetStep;
			vector<PListType>::const_iterator last = pListBuffer.end();
			vector<PListType> tempPListBuffer(first, last);
			//If written clear the pList out
			pListBuffer.clear();
			//append rest of buffer if not empty
			if(!tempPListBuffer.empty())
			{
				pListBuffer.insert(pListBuffer.end(), tempPListBuffer.begin(), tempPListBuffer.end());
				tempPListBuffer.clear();
			}
			mappingIndex = prevMappingIndex;
		}
		if(flush)
		{
			pListBuffer.shrink_to_fit();
		}
	}
	catch(exception e)
	{
		cout << "Exception occurred in method WriteArchiveMapMMAPMeta -> " << e.what() << endl;
	}
}

vector<string>* PListArchive::GetPatterns(unsigned int level, PListType count)
{
	PListType preservedFileIndex = fileIndex;

	long long result;
	char *mapChar;  /* mmapped array of char's */

	//If file size hasn't been found yet grab it once
	if(fileSize == -1)
	{
		fileSize = MemoryUtils::FileSize(this->fileName);
	}

	//Add plus two to account for patternLength and mapEntries.
	PListType sizeToReadForPatterns = count*level;
	PListType totalReadsForPatterns = ceil(((double)(sizeToReadForPatterns))/((double)hdSectorSize)) + 1;
	vector<string> *newStringBuffer = new vector<string>(count);
	PListType newStringBufferIndex = 0;
	PListType offstep = 0;
	unsigned int prevIndexForChar = 0;

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
				close(fd);
				perror("Error mmapping the file");
			}
		
			PListType PListBuffSize = hdSectorSize/sizeof(char);
			/* Now do something with the information. */

			if(piss < totalReadsForPatterns)
			{
				PListBuffSize = hdSectorSize/sizeof(char);

				bool shitWhistle = false;
				PListType i = 0;
				while (i < PListBuffSize && offstep < count) 
				{
					for(PListType charIt = prevIndexForChar; charIt < level; charIt++)
					{
						if(i >= PListBuffSize)
						{
							prevIndexForChar = charIt;
							shitWhistle = true;
							break;
						}
						else
						{
							prevIndexForChar = 0;
						}
					
						if(!shitWhistle)
						{
							completePattern[charIt] = mapChar[i];
						}
						i++;
					}
					if(!shitWhistle)
					{ 
						(*newStringBuffer)[newStringBufferIndex++] = completePattern;
						offstep++;
					}
				}

				if (munmap(mapChar, PListBuffSize) == -1) 
				{
					perror("Error un-mmapping the file");
				}
			}
		
			fileIndex += hdSectorSize;
		}
		fileIndex = preservedFileIndex;
		newStringBuffer->shrink_to_fit();
	}
	catch(exception e)
	{
		cout << "Exception occurred in method GetPatterns -> " << e.what() << endl;
	}
	return newStringBuffer;
}

void PListArchive::ReadMemoryMapMMAPFromDisk()
{
	try
	{
		//USE pListMetaData NOW!!!


		long long result;
		PListType *mapPListType;  /* mmapped array of PListType's */
		char *mapChar;  /* mmapped array of char's */

		double InitMemoryMB = MemoryUtils::GetProgramMemoryConsumption();

		//If file size hasn't been found yet grab it once
		if(fileSize == -1)
		{
			fileSize = MemoryUtils::FileSize(this->fileName);
		}

		//FIRST find pattern length
	#if defined(_WIN64) || defined(_WIN32)
		result = _lseeki64(fd, fileIndex, SEEK_SET);
	#elif defined(__linux__)
		result = lseek64(fd, fileIndex, SEEK_SET);
	#endif
	
		mapPListType = (PListType *)mmap64 (0, hdSectorSize, PROT_READ, MAP_SHARED, fd, fileIndex);

		PListType patternLength = mapPListType[0];
		PListType mapEntries = mapPListType[1];

		if (munmap(mapPListType, hdSectorSize/sizeof(PListType)) == -1) 
		{
			perror("Error un-mmapping the file");
		}

		//Add plus two to account for patternLength and mapEntries.
		PListType sizeToReadForPLists = (mapEntries + 2)*sizeof(PListType);
		PListType sizeToReadForPatterns = mapEntries*patternLength;

		//Start after patternLength and mapEntries are parsed
		PListType startIndex = 2;
	
		PListType totalReadsForPList = ceil(((double)(sizeToReadForPLists))/((double)hdSectorSize)) + 1;
		PListType totalReadsForPatterns = ceil(((double)(sizeToReadForPatterns))/((double)hdSectorSize)) + 1;
	
		PListType pListGlobalIndex = -1;
		PListType listCount = 2;

		vector<PListType> *pListCollector = new vector<PListType>();

		for(PListType piss = 0; piss < totalReadsForPList; piss++)
		{

	#if defined(_WIN64) || defined(_WIN32)
			result = _lseeki64(fd, fileIndex, SEEK_SET);
	#elif defined(__linux__)
			result = lseek64(fd, fileIndex, SEEK_SET);
	#endif
	
			mapPListType = (PListType *)mmap64 (0, hdSectorSize, PROT_READ, MAP_SHARED, fd, fileIndex);

			if (mapPListType == MAP_FAILED) 
			{
				close(fd);
				perror("Error mmapping the file");
			}
		
			PListType PListBuffSize = hdSectorSize/sizeof(PListType);
			/* Now do something with the information. */

			if(piss < totalReadsForPList)
			{
				PListBuffSize = hdSectorSize/sizeof(PListType);

				if((sizeToReadForPLists - (piss*hdSectorSize)) < hdSectorSize)
				{
					PListBuffSize = ((sizeToReadForPLists - (piss*hdSectorSize)))/sizeof(PListType);
				}
			
				for (PListType i = startIndex; i < PListBuffSize && listCount < mapEntries + 2; i++) 
				{
					pListCollector->push_back(mapPListType[i]);
					listCount++;
				}

				startIndex = 0;
			
				if (munmap(mapPListType, PListBuffSize) == -1) 
				{
					perror("Error un-mmapping the file");
				}
			}
		
			fileIndex += hdSectorSize;
		}

		cout << "Patterns read in: " << pListCollector->size() << endl;

		PListType offstep = 0;

		unsigned int prevIndexForChar = 0;
	
		string completePattern = "";

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
				close(fd);
				perror("Error mmapping the file");
			}
		
			PListType PListBuffSize = hdSectorSize/sizeof(char);
			/* Now do something with the information. */

			if(piss < totalReadsForPatterns)
			{
				PListBuffSize = hdSectorSize/sizeof(char);

				bool shitWhistle = false;
				PListType i = 0;
				while (i < PListBuffSize && offstep < mapEntries) 
				{
					for(PListType charIt = prevIndexForChar; charIt < patternLength; charIt++)
					{
						if(i >= PListBuffSize)
						{
							prevIndexForChar = charIt;
							shitWhistle = true;
							break;
						}
						else
						{
							prevIndexForChar = 0;
						}
					
						if(!shitWhistle)
						{
							completePattern.push_back(mapChar[i]);
						}
						i++;
					}
					if(!shitWhistle)
					{ 
						if(pListMetaData.find(completePattern) != pListMetaData.end())
						{
							cout << "we have a duplicate wtf??!!" << endl;
						}
						pListMetaData[completePattern] = (*pListCollector)[offstep];
						offstep++;
						completePattern = "";
					}
				}

				if (munmap(mapChar, PListBuffSize) == -1) 
				{
					perror("Error un-mmapping the file");
				}
			}
			fileIndex += hdSectorSize;
		}
		delete pListCollector;

	}
	catch(exception e)
	{
		cout << "Exception occurred in method ReadMemoryMapMMAPFromDisk -> " << e.what() << endl;
	}
}

void PListArchive::DumpPatternsToDisk(unsigned int level)
{
	try
	{
		long long result;
		
		typedef std::map<PatternType, PListType>::iterator it_map_type;
		char *mapForChars = NULL;  /* mmapped array of char's */

		string file;
	
		file.append("../Log/");
		std::string::size_type i = fileName.find(".txt");
		string tempString = fileName;
		tempString.erase(i, 4);
		file.append(tempString);
		file.append("Patterns.txt");

		ofstream outputFile(file);
		outputFile.close();

		int mapFD =  open(file.c_str(), O_RDWR );
	
		bool doneWithThisShit = false;

		//overshoot how much we are going to write just in case by adding one extra to the offset
		// i don't know why but it makes it work because I believe before not everything was being written or something was getting overwritten
		// who fucking knows but baby does it work now!
		PListType totalWritesForCharTypes = ceil(((double)(stringBuffer.size()*level))/((double)hdSectorSize)) + 1;

		//If offset is less than disk write size then write whatever can be done
		if(totalWritesForCharTypes == 0 )
		{
			totalWritesForCharTypes = 1;
		}

		PListType finalWriteSize = (totalWritesForCharTypes)*hdSectorSize; 

		//Reset final Index
		fileIndex = 0;

		/* stretch the file size to the size of the (mmapped) array of unsigned char
			*/
	#if defined(_WIN64) || defined(_WIN32)
		result = _lseeki64(mapFD, fileIndex + finalWriteSize - 1, SEEK_SET);
	#elif defined(__linux__)
		result = lseek64(mapFD, fileIndex + finalWriteSize - 1, SEEK_SET);
	#endif
		
		if (result == -1) 
		{
			close(mapFD);
			perror("error calling lseek() to 'stretch' the file");
			exit(0);
		}
    
		/* something needs to be written at the end of the file to
			* have the file actually have the new size.
			* just writing an empty string at the current file position will do.
			*
			* note:
			*  - the current position in the file is at the end of the stretched 
			*    file due to the call to lseek().
			*  - an empty string is actually a single '\0' character, so a zero-byte
			*    will be written at the last byte of the file.
			*/
		result = write(mapFD, "", 1);
		if (result != 1) 
		{
			close(mapFD);
			perror("error writing last byte of the file");
			exit(0);
		}

		bool firstWritePatternLength = false;

	
		unsigned int prevIndexForChar = 0;
		PListType prevIndexForString = 0;
		bool unfinished = false;
		PListType stringIndex = 0;
		
		for(int i = 0; i < totalWritesForCharTypes && !doneWithThisShit; i++)
		{
			mapForChars = (char *)mmap64(0, hdSectorSize, PROT_WRITE, MAP_SHARED, mapFD, fileIndex);
		
			if (mapForChars == MAP_FAILED) 
			{
				close(mapFD);
				perror("Error mmapping the file");
				exit(0);
			}

			stringIndex = prevIndexForString;

			bool shitWhistle = false;
			unsigned int z = 0;
			
			while(stringIndex < stringBuffer.size() && z < (hdSectorSize/sizeof(char)) && !doneWithThisShit)
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
					doneWithThisShit = true;
				}
			}	

			//Flush pages to hard disk
			msync(mapForChars, hdSectorSize, MS_SYNC);

			fileIndex += hdSectorSize;
		
			/* Don't forget to free the mmapped memory
				*/

			if (munmap(mapForChars, hdSectorSize) == -1) 
			{
				perror("Error un-mmapping the file");
				/* Decide here whether to close(fd) and exit() or not. Depends... */
				close(mapFD);
				exit(0);
			}
		}
		
		close(mapFD);
	}
	catch(exception e)
	{
		cout << "Exception occurred in method DumpPatternsToDisk -> " << e.what() << endl;
	}
}

//Write map to hard disk 
void PListArchive::DumpMemoryMapMMAPToDisk()
{
	try
	{
		long long result;
		
		typedef std::map<PatternType, PListType>::iterator it_map_type;
		PListType *mapForPListType = NULL;  /* mmapped array of PListTypes */
		char *mapForChars = NULL;  /* mmapped array of char's */

		string file;
	
		file.append("../Log/");
		std::string::size_type i = fileName.find(".txt");
		string tempString = fileName;
		tempString.erase(i, 4);
		file.append(tempString);
		file.append("Map.txt");

		ofstream outputFile(file);
		outputFile.close();

		int mapFD =  open(file.c_str(), O_RDWR );
	
		bool doneWithThisShit = false;
		PListType pMetaDataSize = pListMetaData.size();

		//overshoot how much we are going to write just in case by adding one extra to the offset
		// i don't know why but it makes it work because I believe before not everything was being written or something was getting overwritten
		// who fucking knows but baby does it work now!
		PListType totalWritesForPListTypes = ceil(((double)((pMetaDataSize + 2)*sizeof(PListType)))/((double)hdSectorSize)) + 1; //Add 2 to tell how long each pattern length is and how many patterns in file
	
		PListType totalWriteForPatternsSizeInMB = 0;
		PListType patternLength = 0;
		if(pListMetaData.size() > 0)
		{
			patternLength = pListMetaData.begin()->first.size();
			totalWriteForPatternsSizeInMB = pMetaDataSize*patternLength; 
		}
		else
		{
			return;
		}
	
		//overshoot how much we are going to write just in case by adding one extra to the offset
		// i don't know why but it makes it work because I believe before not everything was being written or something was getting overwritten
		// who fucking knows but baby does it work now!
		PListType totalWritesForCharTypes = ceil(((double)(totalWriteForPatternsSizeInMB))/((double)hdSectorSize)) + 1;

		//If offset is less than disk write size then write whatever can be done
		if(totalWritesForPListTypes == 0 )
		{
			totalWritesForPListTypes = 1;
		}
		//If offset is less than disk write size then write whatever can be done
		if(totalWritesForCharTypes == 0 )
		{
			totalWritesForCharTypes = 1;
		}
		PListType finalWriteSize = (totalWritesForPListTypes + totalWritesForCharTypes)*hdSectorSize; 

		it_map_type globalMapIterator = pListMetaData.begin();

		//Reset final Index
		fileIndex = 0;

		/* stretch the file size to the size of the (mmapped) array of unsigned char
			*/
	#if defined(_WIN64) || defined(_WIN32)
		result = _lseeki64(mapFD, fileIndex + finalWriteSize - 1, SEEK_SET);
	#elif defined(__linux__)
		result = lseek64(mapFD, fileIndex + finalWriteSize - 1, SEEK_SET);
	#endif
		
		if (result == -1) 
		{
			close(mapFD);
			perror("error calling lseek() to 'stretch' the file");
			exit(0);
		}
    
		/* something needs to be written at the end of the file to
			* have the file actually have the new size.
			* just writing an empty string at the current file position will do.
			*
			* note:
			*  - the current position in the file is at the end of the stretched 
			*    file due to the call to lseek().
			*  - an empty string is actually a single '\0' character, so a zero-byte
			*    will be written at the last byte of the file.
			*/
		result = write(mapFD, "", 1);
		if (result != 1) 
		{
			close(mapFD);
			perror("error writing last byte of the file");
			exit(0);
		}

		bool firstWritePatternLength = false;
	
		//First write PListType indexes
		for(int i = 0; i < totalWritesForPListTypes && !doneWithThisShit; i++)
		{
			mapForPListType = (PListType *)mmap64(0, hdSectorSize, PROT_WRITE, MAP_SHARED, mapFD, fileIndex);
			if (mapForPListType == MAP_FAILED) 
			{
				close(mapFD);
				perror("Error mmapping the file");
				exit(0);
			}

			unsigned int z = 0;
			//To write meta data first before map data is written
			if(!firstWritePatternLength)
			{
				//First write pattern lengths
				mapForPListType[z] = patternLength;
				z++;
				//Second write number of map entries
				mapForPListType[z] = pMetaDataSize;
				z++;
				firstWritePatternLength = true;
			}
			while(globalMapIterator != pListMetaData.end() && z < (hdSectorSize/sizeof(PListType)))
			{
				mapForPListType[z] = globalMapIterator->second;
				globalMapIterator++;
				z++;

				if(globalMapIterator == pListMetaData.end())
				{
					doneWithThisShit = true;
				}
			}

			//Flush pages to hard disk
			msync(mapForPListType, hdSectorSize, MS_SYNC);

			fileIndex += hdSectorSize;
		
			/* Don't forget to free the mmapped memory
				*/

			if (munmap(mapForPListType, hdSectorSize) == -1) 
			{
				perror("Error un-mmapping the file");
				/* Decide here whether to close(fd) and exit() or not. Depends... */
				close(mapFD);
				exit(0);
			}
		}

		//Second write Pattern indexes
		doneWithThisShit = false;
		globalMapIterator = pListMetaData.begin();
		fileIndex = totalWritesForPListTypes*hdSectorSize;

		unsigned int prevIndexForChar = 0;
		it_map_type prevItForChar = pListMetaData.begin();
		bool unfinished = false;
	
		for(int i = 0; i < totalWritesForCharTypes && !doneWithThisShit; i++)
		{
			mapForChars = (char *)mmap64(0, hdSectorSize, PROT_WRITE, MAP_SHARED, mapFD, fileIndex);

			if (mapForChars == MAP_FAILED) 
			{
				close(mapFD);
				perror("Error mmapping the file");
				exit(0);
			}

			globalMapIterator = prevItForChar;

			bool shitWhistle = false;
			unsigned int z = 0;
			
			while(globalMapIterator != pListMetaData.end() && z < (hdSectorSize/sizeof(char)) && !doneWithThisShit)
			{
				int patternIt = prevIndexForChar;
				while(patternIt < patternLength && z < (hdSectorSize/sizeof(char)) )
				{
					mapForChars[z] = globalMapIterator->first[patternIt];
					z++;
					patternIt++;
				}
				
				if(patternIt == patternLength)
				{
					prevIndexForChar = 0;
					globalMapIterator++;
				}
				else
				{
					prevIndexForChar = patternIt;
				}
				
				prevItForChar = globalMapIterator;
				
				if(globalMapIterator == pListMetaData.end())
				{
					cout << "File index finished at: " << fileIndex + z << endl;
					doneWithThisShit = true;
				}
			}	

			//Flush pages to hard disk
			msync(mapForChars, hdSectorSize, MS_SYNC);

			fileIndex += hdSectorSize;
		
			/* Don't forget to free the mmapped memory
				*/

			if (munmap(mapForChars, hdSectorSize) == -1) 
			{
				perror("Error un-mmapping the file");
				/* Decide here whether to close(fd) and exit() or not. Depends... */
				close(mapFD);
				exit(0);
			}
		}
		close(mapFD);
	}
	catch(exception e)
	{
		cout << "Exception occurred in method DumpMemoryMapMMAPToDisk -> " << e.what() << endl;
	}
	
}

vector<PListType>* PListArchive::GetListFromIndex(PListType index)
{
	long long result;
	PListType *map;  /* mmapped array of char's */

	PListType offSetOfPlacement = (index % hdSectorSize) / sizeof(PListType);
	PListType numReads = ((double)index) / ((double)hdSectorSize);
	PListType placementIndex = numReads * hdSectorSize;
	PListType pListsToRead = 0;
	
	vector<PListType>* stuffedPListBuffer = new vector<PListType>();

	try
	{
#if defined(_WIN64) || defined(_WIN32)
		result = _lseeki64(fd, placementIndex, SEEK_SET);
#elif defined(__linux__)
		result = lseek64(fd, placementIndex, SEEK_SET);
#endif
	
		map = (PListType *)mmap64 (0, hdSectorSize, PROT_READ, MAP_SHARED, fd, placementIndex);

		if (map == MAP_FAILED) 
		{
			close(fd);
			perror("Error mmapping the file");
		}
	
		pListsToRead = map[offSetOfPlacement];

		if (munmap(map, hdSectorSize) == -1) 
		{
			perror("Error un-mmapping the file");
		}
	
		//If file size hasn't been found yet grab it once
		if(fileSize == -1)
		{
			fileSize = MemoryUtils::FileSize(this->fileName);
		}

		PListType sizeToRead = (pListsToRead + offSetOfPlacement)*sizeof(PListType);
		if(sizeToRead > fileSize - placementIndex)
		{
			sizeToRead = fileSize - placementIndex;
		}
		PListType pListGlobalIndex = -1;
		PListType listCount = 0;
		bool alreadyGrabbed = false;
		bool doneProcessing = false;

		while(!doneProcessing)
		{

	#if defined(_WIN64) || defined(_WIN32)
			result = _lseeki64(fd, placementIndex, SEEK_SET);
	#elif defined(__linux__)
			result = lseek64(fd, placementIndex, SEEK_SET);
	#endif
	
			map = (PListType *)mmap64 (0, hdSectorSize, PROT_READ, MAP_SHARED, fd, placementIndex);

			if (map == MAP_FAILED) 
			{
				close(fd);
				perror("Error mmapping the file");
			}
		
			PListType PListBuffSize = hdSectorSize/sizeof(PListType);
			/* Now do something with the information. */

			PListBuffSize = hdSectorSize/sizeof(PListType);

			for (PListType i = offSetOfPlacement; i < PListBuffSize; i++) 
			{
				if(listCount == 0)
				{
					//After using originial offset don't need it no more
					offSetOfPlacement = 0;
					if(alreadyGrabbed)
					{
						doneProcessing = true;
						break;
					}
					listCount = (PListType) map[i];
					//cout << "Size of pList: " << listCount << endl;
					alreadyGrabbed = true;
					//If listCount equals zero then we are at the end of the pList data stream bam bitches
					if(listCount == 0)
					{
						break;
					}
				
				}
				else
				{
					stuffedPListBuffer->push_back(map[i]);
					listCount--;
				}
			}

			if (munmap(map, PListBuffSize) == -1) 
			{
				perror("Error un-mmapping the file");
			}
		
			if(!doneProcessing)
			{
				placementIndex += hdSectorSize;
			}
		}
	}
	catch(exception e)
	{
		cout << "Exception occurred in method GetListFromIndex -> " << e.what() << endl;
	}
	
	return stuffedPListBuffer;
}


map<PatternType, PListType> PListArchive::GetMetaDataMap()
{
	return pListMetaData;
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
		cout << "Exception occurred in method CloseArchiveMMAP -> " << e.what() << endl;
	}
}