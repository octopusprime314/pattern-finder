#include "PListArchive.h"
#include "MemoryUtils.h"
#include <exception>
#include "Forest.h"

vector<thread*> PListArchive::threadKillList;
mutex PListArchive::syncLock;
mutex PListArchive::listCountMutex;

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
		
		file.append("../Log/");
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
				fd = open(file.c_str(), O_RDWR | O_TRUNC);
			}
		}
		else
		{
			fd = open(file.c_str(), O_RDWR);
		}

		this->fileName = file;
		if(fd < 0)
		{
			
			stringstream stringbuilder;
			stringbuilder << file.c_str() << " file not found!";
			stringbuilder << " and errno is "<< errno << endl;
			Logger::WriteLog(stringbuilder.str());
			endOfFileReached = true;
			return;
		}
		
		fileSize = -1;
		fileIndex = 0;
		startingIndex = 0;
		mappingIndex = 0;
		prevMappingIndex = 0;
		predictedFutureMemoryLocation = 0;
	}
	catch(exception e)
	{
		cout << "Exception occurred in method PListArchive constructor -> " << e.what() << endl;
	}
}
PListArchive::~PListArchive(void)
{
}

void PListArchive::MappingError(int& fileDescriptor, string fileName)
{
	close(fileDescriptor);
	fileDescriptor = -1;
	stringstream handle;
	handle << "error mapping the file " << fileName << endl;
	Logger::WriteLog(handle.str());
}
	
void PListArchive::UnMappingError(int& fileDescriptor, string fileName)
{
	close(fileDescriptor);
	fileDescriptor = -1;
	stringstream handle;
	handle << "error un-mapping the file " << fileName << endl;
	Logger::WriteLog(handle.str());
}

void PListArchive::SeekingError(int& fileDescriptor, string fileName)
{
	close(fileDescriptor);
	fileDescriptor = -1;
	stringstream handle;
	handle << "error calling lseek() to 'stretch' the file " << fileName << endl;
	Logger::WriteLog(handle.str());
}

void PListArchive::ExtendingFileError(int& fileDescriptor, string fileName)
{
	close(fileDescriptor);
	fileDescriptor = -1;
	stringstream handle;
	handle << "error writing last byte of the file " << fileName << endl;
	Logger::WriteLog(handle.str());
}

//void PListArchive::GetPListArchiveMMAP(vector<vector<PListType>*> &stuffedPListBuffer, PListType chunkSizeInMB)
//{
//	long long result;
//	PListType *map;  /* mmapped array of char's */
//
//	double InitMemoryMB = MemoryUtils::GetProgramMemoryConsumption();
//	
//	PListType sizeToRead = 0;
//	if(chunkSizeInMB == 0)
//	{
//		sizeToRead = fileSize;
//	}
//	else
//	{
//		sizeToRead = chunkSizeInMB*1000000;
//	}
//
//	if(sizeToRead > fileSize - fileIndex)
//	{
//		sizeToRead = fileSize - fileIndex;
//	}
//	PListType totalReads = ((double)sizeToRead)/((double)hdSectorSize);
//	PListType pListGlobalIndex = -1;
//	PListType listCount = 0;
//
//	if(totalReads < 1 && fileSize > 0)
//	{
//		totalReads = 1;
//	}
//	else if(totalReads < 1 && fileSize == 0)
//	{
//		totalReads = 0;
//	}
//
//	PListType globalTotalMemoryInBytes = 0;
//	
//	PListType totalCount = 0;
//	try
//	{
//		bool finishedFlag = false;
//
//		for(PListType piss = 0; piss < totalReads && !finishedFlag; piss++)
//		{
//			map = (PListType *)mmap64 (0, hdSectorSize, PROT_READ, MAP_SHARED, fd, fileIndex);
//	
//			if (map == MAP_FAILED) 
//			{
//				//endOfFileReached = true;
//				MappingError(fd, this->fileName);
//				return;
//			}
//		
//			PListType PListBuffSize = hdSectorSize/sizeof(PListType);
//			/* Now do something with the information. */
//
//			if(piss < totalReads)
//			{
//				PListBuffSize = hdSectorSize/sizeof(PListType);
//
//				if((sizeToRead - (piss*hdSectorSize)) < hdSectorSize)
//				{
//					PListBuffSize = ((sizeToRead - (piss*hdSectorSize)))/sizeof(PListType);
//				}
//				if(startingIndex != 0)
//				{
//					PListBuffSize = hdSectorSize/sizeof(PListType) - startingIndex;
//				}
//			
//				for (PListType i = startingIndex; i < PListBuffSize + startingIndex; i++) 
//				{
//					if(listCount == 0)
//					{
//						if(pListGlobalIndex != -1)
//						{
//							stuffedPListBuffer[pListGlobalIndex]->shrink_to_fit();
//
//							if(chunkSizeInMB != 0)
//							{
//
//								//size of vector container
//								globalTotalMemoryInBytes += 24;
//								//Pointer size
//								globalTotalMemoryInBytes += 8;
//								//Size of total vector on the heap
//								globalTotalMemoryInBytes += stuffedPListBuffer[pListGlobalIndex]->capacity()*sizeof(PListType);
//
//								//double CurrentMemoryMB = MemoryUtils::GetProgramMemoryConsumption() - InitMemoryMB;
//								//if(CurrentMemoryMB >= chunkSizeInMB)
//								if(/*Forest::overMemoryCount || */((globalTotalMemoryInBytes + (stuffedPListBuffer.capacity()*24))/1000000.0f) >= chunkSizeInMB)
//								{
//									stringstream strim;
//									strim << "Approximated size of vector: " << ((globalTotalMemoryInBytes + (stuffedPListBuffer.capacity()*24))/1000000.0f) << " in MB!" << endl;
//									Logger::WriteLog(strim.str());
//									finishedFlag = true;
//									
//									/*if(stuffedPListBuffer.size() > 1)
//									{*/
//										fileIndex = prevListIndex;
//										startingIndex = prevStartingIndex;
//										delete stuffedPListBuffer[pListGlobalIndex];
//										stuffedPListBuffer.pop_back();
//									//}
//									//cout << "Didn't make it to the end of file: " << fileName << endl;
//									break;
//								}
//							}
//						}
//
//						listCount = (PListType) map[i];
//						totalCount = listCount;
//						//cout << "List count: " << listCount << endl;
//						prevListIndex = fileIndex;
//						prevStartingIndex = i;
//
//						//If listCount equals zero then we are at the end of the pList data stream bam bitches
//						if(listCount == 0)
//						{
//							finishedFlag = true;
//							endOfFileReached = true;
//							break;
//						}
//						stuffedPListBuffer.push_back(new vector<PListType>());
//						
//						pListGlobalIndex++;
//						//
//						stuffedPListBuffer[pListGlobalIndex]->reserve(listCount);
//						//
//					}
//					else
//					{
//						stuffedPListBuffer[pListGlobalIndex]->push_back(map[i]);
//						listCount--;
//					}
//				}
//
//				if(startingIndex != 0 && !finishedFlag)
//				{
//					startingIndex = 0;
//				}
//
//				/*if(listCount > 0 && piss == totalReads - 1 && !finishedFlag)
//				{
//					totalReads++;
//				}*/
//
//				//If pattern is not done we have to resize again
//				if(listCount > 0 && piss == totalReads - 1 && !finishedFlag)
//				{
//					if(chunkSizeInMB != 0)
//					{
//						
//						finishedFlag = true;
//						/*if(stuffedPListBuffer.size() > 1)
//						{*/
//							fileIndex = prevListIndex;
//							startingIndex = prevStartingIndex;
//							delete stuffedPListBuffer[pListGlobalIndex];
//							stuffedPListBuffer.pop_back();
//						//}
//						//cout << "Didn't make it to the end of file: " << fileName << endl;
//					}
//				}
//				//if size of chunk is less than hdSector size
//				else if(sizeToRead <= hdSectorSize/*startingIndex == 0*/)
//				{
//					fileIndex += hdSectorSize;
//				}
//				if (munmap(map, PListBuffSize) == -1) 
//				{
//					UnMappingError(fd, this->fileName);
//					return;
//				}
//			}
//		
//			if(!finishedFlag /*&& startingIndex != 0*/)
//			{
//				fileIndex += hdSectorSize;
//			}
//		
//		}
//	}
//	catch(exception e)
//	{
//		cout << "Exception occurred in method GetPListArchiveMMAP -> " << e.what() << endl;
//	}
//}

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

							if(/*Forest::overMemoryCount || */((globalTotalMemoryInBytes + (stuffedPListBuffer.capacity()*24))/1000000.0f) >= chunkSizeInMB)
							{
								stringstream strim;
								strim << "Approximated size of vector: " << ((globalTotalMemoryInBytes + (stuffedPListBuffer.capacity()*24))/1000000.0f) << " in MB!" << endl;
								Logger::WriteLog(strim.str());
								finishedFlag = true;
								
									
								if(stuffedPListBuffer.size() > 1)
								{
									trimPList = true;

									fileIndex = prevListIndex;
									startingIndex = prevStartingIndex;

									delete stuffedPListBuffer[pListGlobalIndex];
									stuffedPListBuffer.pop_back();
								}
								//cout << "Didn't make it to the end of file: " << fileName << endl;
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
					//
					stuffedPListBuffer[pListGlobalIndex]->reserve(listCount);
					//
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
		cout << "Exception occurred in method GetPListArchiveMMAP -> " << e.what() << endl;
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

void PListArchive::FlushMapList(list<PListType*> memLocalList, list<char*> charLocalList, PListType *mapToDelete)
{
	
	//memLocalList.unique();
	for(PListType* temp : memLocalList)
	{
		msync(temp, hdSectorSize, MS_SYNC);
	}
	for(char* tempChar : charLocalList)
	{
		msync(tempChar, hdSectorSize, MS_SYNC);
	}
	
	
}

void PListArchive::WriteArchiveMapMMAP(const vector<PListType> &pListVector, PatternType pattern, bool flush)
{
	try
	{
		if(flush)
		{
			//Kick off thread that flushes cached memory mapping to disk asynchronously and it may be bad lol
			//cout << "Number of memory locations to flush: " << memLocals.size() << endl;
			totalWritten = 0;
			syncLock.lock();
			threadKillList.push_back(new thread(&PListArchive::FlushMapList, this, memLocals, charLocals, mapper));
			syncLock.unlock();
			memLocals.clear();
			if(mapper != NULL)
			{
				//Deallocate only when it has been completely used
				if (munmap(mapper, hdSectorSize) == -1) 
				{
					UnMappingError(fd, this->fileName);
					return;
				}
				mapper = NULL;
			}
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

		bool doneWithThisShit = false;

		//overshoot how much we are going to write just in case by adding one extra to the offset
		// i don't know why but it makes it work because I believe before not everything was being written or something was getting overwritten
		// who fucking knows but baby does it work now!
		//PListType offset = ceil(((double)(pListBuffer.size()*sizeof(PListType)))/((double)hdSectorSize)) + 1;
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
			write(fd, "", 1);
			prevFileIndex = fileIndex + (offset*hdSectorSize) - 1;
		}
	#endif

		if(predictedFutureMemoryLocation != fileIndex + (startPoint*sizeof(PListType)))
		{
			PListType woAdjustment = startPoint;
			cout << "Prediction conflict at file " << this->fileName << endl;
			cout << "Predicted location " << predictedFutureMemoryLocation << " and actual " << fileIndex + (startPoint*sizeof(PListType)) << endl;
			startPoint = (predictedFutureMemoryLocation%hdSectorSize)/(sizeof(PListType));
			//fileIndex = (predictedFutureMemoryLocation/hdSectorSize)*hdSectorSize;
			cout << "Adjusted location: " << startPoint << endl;
		}

		predictedFutureMemoryLocation = fileIndex + ((startPoint+1+pListSize)*sizeof(PListType));

		int i;
		for(i = 0; i < offset && !doneWithThisShit && pListSize > 0; i++)
		{
			if(startPoint == 0 || mapper == NULL)
			{
				if(mapper != NULL)
				{
					//Deallocate only when it has been completely used
					if (munmap(mapper, hdSectorSize) == -1) 
					{
						UnMappingError(fd, this->fileName);
						return;
					}
					mapper = NULL;
				}

				mapper = (PListType *)mmap64(0, hdSectorSize, PROT_WRITE, MAP_SHARED, fd, fileIndex);
			
				memLocals.push_back(mapper);
				
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
					doneWithThisShit = true;
					break;
				}
			}


			//No longer need to use a starting point after first write hehe :)
			startPoint = 0;

			if(z == totalLoops)
			{
				

				fileIndex += hdSectorSize;
				////Deallocate only when it has been completely used
				//if (munmap(mapper, hdSectorSize) == -1) 
				//{
				//	UnMappingError(fd, this->fileName);
				//	return;
				//}
				//mapper = NULL;
			}

		}

		
		mappingIndex = prevMappingIndex;

	}
	catch(exception e)
	{
		cout << "Exception occurred in method WriteArchiveMapMMAP -> " << e.what() << endl;
		cout << "Vector size man! " << pListVector.size() << endl;
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
		cout << "Exception occurred in method GetPatterns -> " << e.what() << endl;
	}
	return newStringBuffer;
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

		bool firstWritePatternLength = false;

		unsigned int prevIndexForChar = 0;
		PListType prevIndexForString = 0;
		bool unfinished = false;
		PListType stringIndex = 0;

	#if defined(__linux__)
		result = lseek64(mapFD, fileIndex + finalWriteSize - 1, SEEK_SET);
		write(mapFD, "", 1);
	#endif
	
		
		for(int i = 0; i < totalWritesForCharTypes && !doneWithThisShit; i++)
		{
			mapForChars = (char *)mmap64(0, hdSectorSize, PROT_WRITE, MAP_SHARED, mapFD, fileIndex);

			charLocals.push_back(mapForChars);
		
			if (mapForChars == MAP_FAILED) 
			{
				MappingError(mapFD, file);
				return;
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

			fileIndex += hdSectorSize;
		
			/* Don't forget to free the mmapped memory
				*/

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
		cout << "Exception occurred in method DumpPatternsToDisk -> " << e.what() << endl;
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
			/*stringstream stringbuilder;
			stringbuilder << fileName.c_str() << " closed!" << endl;
			Logger::WriteLog(stringbuilder.str());*/
			close(fd);
		}
		/*else
		{
			stringstream stringbuilder;
			stringbuilder << fileName.c_str() << " already closed!" << endl;
			Logger::WriteLog(stringbuilder.str());
		}*/
	}
	catch(exception e)
	{
		cout << "Exception occurred in method CloseArchiveMMAP -> " << e.what() << endl;
	}
}

