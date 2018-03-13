/*
 * Copyright (c) 2016, 2017
* The University of Rhode Island
 * All Rights Reserved
 *
 * This code is part of the Pattern Finder.
 *
 * Permission is granted to use, copy, create derivative works and
 * redistribute this software and such derivative works for any
 * purpose, so long as the copyright notice above, this grant of
 * permission, and the disclaimer below appear in all copies made; and
 * so long as the name of The University of Rhode Island is not used in
 * any advertising or publicity pertaining to the use or distribution
 * of this software without specific, written prior authorization.
 *
 * THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION FROM THE
 * UNIVERSITY OF RHODE ISLAND AS TO ITS FITNESS FOR ANY PURPOSE, AND
 * WITHOUT WARRANTY BY THE UNIVERSITY OF RHODE ISLAND OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE. THE UNIVERSITY OF RHODE ISLAND SHALL NOT BE
 * LIABLE FOR ANY DAMAGES, INCLUDING DIRECT, SPECIAL, INDIRECT,
 * INCIDENTAL, OR CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
 * IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGES.
 *
 * Author: Peter J. Morley 
 *          
 */

#include "PListArchive.h"
#include "Logger.h"

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
		_mapper = nullptr;
		_prevFileIndex = 0;

		_endOfFileReached = false;
		patternName = fileName;
		string file;
		
		file.append(LOGGERPATH);
		file.append(fileName);
		file.append(".txt");
		_fd = -1;
		//Create a new file
		if(create)
		{
#if defined(_WIN64) || defined(_WIN32)
			_fd = _open(file.c_str(), O_RDWR | O_CREAT, _S_IREAD | _S_IWRITE);
#elif defined(__linux__)
			_fd = open(file.c_str(), O_RDWR | O_CREAT, 0644);
#endif
			if(_fd == -1)
			{
				Logger::WriteLog("Why are we truncating the file " , file ,
					" and errno is " , strerror(errno), "\n");
#if defined(_WIN64) || defined(_WIN32)
				_fd = _open(file.c_str(), O_RDWR | O_TRUNC);
#elif defined(__linux__)
                _fd = open(file.c_str(), O_RDWR | O_TRUNC);
#endif
			}
		}
		//Open existing file
		else
		{
#if defined(_WIN64) || defined(_WIN32)
			_fd = _open(file.c_str(), O_RDONLY);
#elif defined(__linux__)
            _fd = open(file.c_str(), O_RDONLY);
#endif
		}

		this->fileName = file;
		//If file does not exist or could not be created then we return unhappy
		if(_fd < 0)
		{
			Logger::WriteLog(file.c_str() , " file not found!", 
			" and errno is " , strerror(errno), "\n");
			_endOfFileReached = true;
			return;
		}
		
		_fileIndex = 0;
		_startingIndex = 0;
		mappingIndex = 0;
		prevMappingIndex = 0;
	}
	catch(exception e)
	{
		Logger::WriteLog("Exception occurred in method PListArchive Costructor -> ", e.what(), "\n");
	}
}
PListArchive::~PListArchive(void)
{
}
//Writing to hd error handling methods
void PListArchive::MappingError(int& fileDescriptor, string filename)
{
	Logger::WriteLog(" and errno is ", strerror(errno), "\n"
	, "error mapping the file " , filename , "\n"
	, "file descriptor: " , fileDescriptor , "\n"
	, "end of file reached: " , _endOfFileReached, "\n");

#if defined(_WIN64) || defined(_WIN32)
    _close(fileDescriptor);
#elif defined(__linux__)
    close(fileDescriptor);
#endif

	fileDescriptor = -1;
	_endOfFileReached = true;
}
	
void PListArchive::UnMappingError(int& fileDescriptor, string filename)
{
	Logger::WriteLog(" and errno is ", strerror(errno), "\n"
		, "error un-mapping the file ", filename, "\n"
		, "file descriptor: ", fileDescriptor, "\n"
		, "end of file reached: ", _endOfFileReached, "\n");

#if defined(_WIN64) || defined(_WIN32)
    _close(fileDescriptor);
#elif defined(__linux__)
    close(fileDescriptor);
#endif

	fileDescriptor = -1;
	_endOfFileReached = true;
}

void PListArchive::SeekingError(int& fileDescriptor, string filename)
{
#if defined(_WIN64) || defined(_WIN32)
    _close(fileDescriptor);
#elif defined(__linux__)
    close(fileDescriptor);
#endif

	fileDescriptor = -1;
	_endOfFileReached = true;
	Logger::WriteLog("error calling lseek() to 'stretch' the file " , filename, "\n");
}

void PListArchive::ExtendingFileError(int& fileDescriptor, string filename)
{
#if defined(_WIN64) || defined(_WIN32)
    _close(fileDescriptor);
#elif defined(__linux__)
    close(fileDescriptor);
#endif

	fileDescriptor = -1;
	_endOfFileReached = true;
	Logger::WriteLog("error writing last byte of the file " , filename , "\n");
}

void PListArchive::GetPListArchiveMMAP(vector<vector<PListType>*> &stuffedPListBuffer, double chunkSizeInMB)
{
	//Read in a block of pattern vectors from file to use in RAM
	PListType *map;  /* mmapped array of char's */
	
	PListType pListGlobalIndex = -1;
	PListType listCount = 0;

	PListType globalTotalMemoryInBytes = 0;
	try
	{
		bool finishedFlag = false;
		bool trimPList = false;

		while(!finishedFlag)
		{
			//Create a memory map block of memory to access directly into the file, ie allocating a virtual page of memory for direct read/write
			map = (PListType *)mmap64 (0, hdSectorSize, PROT_READ, MAP_SHARED, _fd, _fileIndex);

			if (map == MAP_FAILED) 
			{
				MappingError(_fd, this->fileName);
				return;
			}
		
			PListType PListBuffSize = hdSectorSize/sizeof(PListType);
			/* Now do something with the information. */
			PListType i;
			
			for (i = _startingIndex; i < PListBuffSize; i++) 
			{
				if(listCount == 0)
				{
					if(pListGlobalIndex != -1)
					{
						//Shrink down vector to actual size 
						stuffedPListBuffer[pListGlobalIndex]->shrink_to_fit();

						if(chunkSizeInMB != 0)
						{
							//size of vector container
							globalTotalMemoryInBytes += 32;
							//Size of total vector on the heap
							globalTotalMemoryInBytes += static_cast<PListType>(stuffedPListBuffer[pListGlobalIndex]->capacity()*sizeof(PListType));

							//Check if requested memory block of the pattern file has been met
							if(((globalTotalMemoryInBytes + (stuffedPListBuffer.capacity()*24))/1000000.0f) >= chunkSizeInMB)
							{
								finishedFlag = true;
										
								if(stuffedPListBuffer.size() > 1)
								{
									trimPList = true;

									_fileIndex = _prevListIndex;
									_startingIndex = _prevStartingIndex;

									delete stuffedPListBuffer[pListGlobalIndex];
									stuffedPListBuffer.pop_back();
								}
								break;
							}
						}
					}

					_prevStartingIndex = i;
					_prevListIndex = _fileIndex;

					//Grab the vector size of pattern indexes and prepare new vector
					listCount = (PListType) map[i];

					//If listCount equals zero then we are at the end of the pList data stream bam bitches
					if(listCount == 0)
					{
						finishedFlag = true;
						_endOfFileReached = true;
						break;
					}
					stuffedPListBuffer.push_back(new vector<PListType>());
						
					pListGlobalIndex++;
					
					stuffedPListBuffer[pListGlobalIndex]->reserve(listCount);
					
				}
				else
				{
					//Read each index into the vector for each pattern based on the listcount
					stuffedPListBuffer[pListGlobalIndex]->push_back(map[i]);
					listCount--;
				}
			}

			//Trim vector if memory has been exceeded
			if(!trimPList)
			{
				if(i == PListBuffSize)
				{
					_startingIndex = 0;
					_fileIndex += hdSectorSize;
				}
				else
				{
					_startingIndex = i;
				}
			}
			if (munmap(map, PListBuffSize) == -1) 
			{
				UnMappingError(_fd, this->fileName);
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
	//See if the read/write pointer in file has hit EOF
	return _endOfFileReached;
}

bool PListArchive::Exists()
{
	//If file descriptor is greater than 0 it is a legit file handle
	if(_fd > 0)
	{
		return true;
	}
	else
	{
		return false;
	}
}

void PListArchive::WriteArchiveMapMMAP(const vector<PListType> &pListVector, const PatternType &pattern, bool flush)
{
	try
	{
		//Force all patterns to be written to the file
		if(flush)
		{
			//Kick off thread that flushes cached memory mapping to disk asynchronously and it may be bad 
			if(_mapper != nullptr)
			{
				//Do not wait for all data to be written to file before executing next line of code
				msync(_mapper, hdSectorSize, MS_ASYNC);
				//Deallocate only when it has been completely used
				if (munmap(_mapper, hdSectorSize) == -1) 
				{
					UnMappingError(_fd, this->fileName);
					return;
				}
				_mapper = nullptr;
			}
			
			totalWritten = 0;
			
			return;
		}
		//Add pattern string to stringbuffer for later creation of the pattern string file
		if(pListVector.size() > 0)
		{
			if(pattern.size() > 0)
			{
				_stringBuffer.push_back(pattern);
			}
			mappingIndex += static_cast<PListType>(((pListVector.size() + 1)*sizeof(PListType)));
		}

		//Compute index of current memory mapping 
		PListType startPoint = ((prevMappingIndex/sizeof(PListType)) % totalLoops);
		PListType tempMapIndex = mappingIndex;
		mappingIndex = prevMappingIndex;
		prevMappingIndex = tempMapIndex;
		
		
		_fileIndex = (mappingIndex/hdSectorSize)*hdSectorSize;

		bool done = false;

		//overshoot how much we are going to write just in case by adding one extra to the offset
		// i don't know why but it makes it work because I believe before not everything was being written or something was getting overwritten
		PListType offset = static_cast<PListType>(ceil(((double)(pListVector.size()*sizeof(PListType)))/((double)hdSectorSize)) + 1);

		//If offset is less than disk write size then write whatever can be done
		if(offset <= 0)
		{
			offset = 1;
		}

		PListType pListSize = static_cast<PListType>(pListVector.size());

		PListType offsetStep = 0;

		bool grabbedCount = false;

		//In linux the file needs to be seeked in order to create more memory for file
	#if defined(__linux__)
		if((_fileIndex + (offset*hdSectorSize) - 1) >= _prevFileIndex)
		{
			lseek64(_fd, _fileIndex + (offset*hdSectorSize) - 1, SEEK_SET);
			size_t writeSize = write(_fd, "", 1);
			_prevFileIndex = _fileIndex + (offset*hdSectorSize) - 1;
		}
	#endif

		PListType i;
		for(i = 0; i < offset && !done && pListSize > 0; i++)
		{
			if(startPoint == 0 || _mapper == nullptr)
			{
				if(_mapper != nullptr)
				{
					//Write back data to file asynchronously ie do not wait
					msync(_mapper, hdSectorSize, MS_ASYNC);
					//Deallocate only when it has been completely used
					if (munmap(_mapper, hdSectorSize) == -1) 
					{
						UnMappingError(_fd, this->fileName);
						return;
					}
					_mapper = nullptr;
				}

				//Allocate a virtual page block of memory to write pattern data to
				_mapper = (PListType *)mmap64(0, hdSectorSize, PROT_WRITE, MAP_SHARED, _fd, _fileIndex);
				
				if (_mapper == MAP_FAILED) 
				{
					MappingError(_fd, this->fileName);
					return;
				}
			}
			/* Now write unsigned longs's to the file as if it were memory (an array of longs).
				*/

			PListType listVectorSize = static_cast<PListType>(pListVector.size());
			PListType z;
			for (z = startPoint; z < totalLoops && offsetStep < listVectorSize; ++z) 
			{
				if(!grabbedCount)
				{
					grabbedCount = true;
					_mapper[z] = static_cast<PListType>(pListVector.size());
				}
				else
				{
					_mapper[z] = pListVector[offsetStep];
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
				_fileIndex += hdSectorSize;
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
	//Gets only the pattern string information from file
	if(_fd == -1)
	{
		return nullptr;
	}

	PListType preservedFileIndex = _fileIndex;

	long long result;
	char *mapChar;  /* mmapped array of char's */

	//Add plus two to account for patternLength and mapEntries.
	PListType sizeToReadForPatterns = count*level;
	PListType totalReadsForPatterns = static_cast<PListType>(ceil(((double)(sizeToReadForPatterns))/((double)hdSectorSize)));
	if(totalReadsForPatterns == 0)
	{
		totalReadsForPatterns++;
	}
	vector<string> *newStringBuffer = nullptr;
	PListType newStringBufferIndex = 0;
	PListType offstep = 0;
	unsigned int prevIndexForChar = 0;

	if(count == 0)
	{
		return nullptr;
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
			result = _lseeki64(_fd, _fileIndex, SEEK_SET);
	#elif defined(__linux__)
			result = lseek64(_fd, _fileIndex, SEEK_SET);
	#endif
			//Allocate a virtual page of memory in the form of a char* array because we are reading in pattern string data
			mapChar = (char *)mmap64 (0, hdSectorSize, PROT_READ, MAP_SHARED, _fd, _fileIndex);

			if (mapChar == MAP_FAILED) 
			{
				MappingError(_fd, this->fileName);
				return nullptr;
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
					//Read in chars until full pattern string is created
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
						//Add new pattern string to string buffer
						(*newStringBuffer)[newStringBufferIndex++] = completePattern;
						offstep++;
					}
				}

				if (munmap(mapChar, PListBuffSize) == -1) 
				{
					UnMappingError(_fd, this->fileName);
					return nullptr;
				}
			}
		
			//Increment next virtual page block to write to
			_fileIndex += hdSectorSize;
		}
		_fileIndex = preservedFileIndex;
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
		//Takes pattern buffer data and creates a pattern string file
		char *mapForChars = nullptr;  /* mmapped array of char's */

		string file;
	
		file.append(LOGGERPATH);
		std::string::size_type index = fileName.find(".txt");
		string tempString = fileName;
		tempString.erase(index, 4);
		stringstream temp;
		temp << tempString;
		temp << "Patterns";
		temp << ".txt";
		file = temp.str();

		//Create file handle for memory map writing
	#if defined(_WIN64) || defined(_WIN32)
		int mapFD = _open(file.c_str(), O_RDWR | O_CREAT, _S_IREAD | _S_IWRITE);
	#elif defined(__linux__)
		int mapFD = open(file.c_str(), O_RDWR | O_CREAT, 0644);
	#endif

		//Make sure the file is created
		if(mapFD < 0)
		{
			Logger::WriteLog(file.c_str() , " map pattern file not found!",
			" and errno is " , strerror(errno) , "\n");
			_endOfFileReached = true;
			return;
		}



		bool done = false;

		//overshoot how much we are going to write just in case by adding one extra to the offset
		// i don't know why but it makes it work because I believe before not everything was being written or something was getting overwritten
		PListType totalWritesForCharTypes = static_cast<PListType>(ceil(((double)(_stringBuffer.size()*level))/((double)hdSectorSize)) + 1);

		//If offset is less than disk write size then write whatever can be done
		if(totalWritesForCharTypes == 0 )
		{
			totalWritesForCharTypes = 1;
		}
		//Reset final Index
		_fileIndex = 0;

		unsigned int prevIndexForChar = 0;
		PListType prevIndexForString = 0;
		PListType stringIndex = 0;

	#if defined(__linux__)
		lseek64(mapFD, fileIndex + finalWriteSize - 1, SEEK_SET);
		size_t writeSize = write(mapFD, "", 1);
	#endif
	
		
		for(PListType i = 0; i < totalWritesForCharTypes && !done; i++)
		{
			//Allocate a virtual page of memory in the form of a char* array because we are writing pattern string data
			mapForChars = (char *)mmap64(0, hdSectorSize, PROT_WRITE, MAP_SHARED, mapFD, _fileIndex);

			if (mapForChars == MAP_FAILED) 
			{
				Logger::WriteLog("fileIndex : " , _fileIndex);
				MappingError(mapFD, file);
				return;
			}

			stringIndex = prevIndexForString;

			unsigned int z = 0;
			
			//Write all string data to a char array
			while(stringIndex < _stringBuffer.size() && z < (hdSectorSize/sizeof(char)) && !done)
			{
				unsigned int patternIt = prevIndexForChar;
				while(patternIt < level && z < (hdSectorSize/sizeof(char)) )
				{
					mapForChars[z] = _stringBuffer[stringIndex][patternIt];
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
				
				if(stringIndex == _stringBuffer.size())
				{
					done = true;
				}
			}	

			_fileIndex += hdSectorSize;
		
			/* Don't forget to free the mmapped memory
				*/
			msync(mapForChars, hdSectorSize, MS_ASYNC);

			if (munmap(mapForChars, hdSectorSize) == -1) 
			{
				UnMappingError(mapFD, file);
				return;
			}
		}
		
#if defined(_WIN64) || defined(_WIN32)
        _close(mapFD);
#elif defined(__linux__)
        close(mapFD);
#endif
	}
	catch(exception e)
	{
		Logger::WriteLog("Exception occurred in method DumpPatternsToDisk -> ", e.what(), "\n");
	}
}


//Close log and save time
void PListArchive::CloseArchiveMMAP()
{
	try
	{
		/* Un-mmaping doesn't close the file, so we still need to do that.
		 */
		if(_fd != -1)
		{
#if defined(_WIN64) || defined(_WIN32)
            _close(_fd);
#elif defined(__linux__)
            close(_fd);
#endif
		}
	}
	catch(exception e)
	{
		Logger::WriteLog("Exception occurred in method CloseArchiveMMAP -> ", e.what(), "\n");
	}
}

