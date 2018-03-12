/** @file PListArchive.h
 *  @brief Writes pattern information to file
 *
 *  The PListArchive class creates files to be read
 *  in and written to for fine grain manipulation.
 *
 *  @author Peter J. Morley (pmorley)
 */

#pragma once

#if defined(_WIN64) || defined(_WIN32)
#include "mman.h"
#include <io.h>
#include <fcntl.h>
#elif defined(__linux__)
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/io.h>
#include <sys/mman.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <math.h>
#endif

class PListArchive
{
public:
	/** @brief Default Constructor.
     */
	PListArchive(void);

	/** @brief Destructor.
     */
	~PListArchive(void);

	/** @brief Creates a file or opens an existing one for pattern data hard disk i/o 
	 *  
	 *  Opens a stream for file operations.
	 *
	 *  @param fileName contains just the file name and not the entire address
	 *  @param create tells PListArchive to create a new file or use an existing file
	 */
	PListArchive(string fileName, bool create = false);

	/** @brief Writes pattern data in vector form to a file 
	 *  
	 *  Writes pattern data in vector form to a file 
	 *
	 *  @param pListVector vector of indexes of a particular pattern
	 *  @param pattern is the contents of the pattern
	 *  @param flush takes all pattern information previously stored and flushes to disk
	 *  @param forceClose closes file stream after writing
	 */
	void WriteArchiveMapMMAP(const vector<PListType> &pListVector, const PatternType &pattern = "", bool flush = false, bool forceClose = false);

	/** @brief Takes all the pattern content information and writes to disk
	 *  
	 *  Takes all the pattern content information and writes to disk
	 *
	 *  @param level current level to know the size of the patterns
	 *  @return void
	 */
	void DumpPatternsToDisk(unsigned int level);

	/** @brief Gets the pattern information from file
	 *  
	 *  Gets the pattern information from file
	 *
	 *  @param level current level to know the size of the patterns
	 *  @param count how many pattern strings to read in before stopping
	 *  @return void
	 */
	vector<string>* GetPatterns(unsigned int level, PListType count);

	/** @brief Gets the pattern index information from file
	 *  
	 *  Gets the pattern index information from file
	 *
	 *  @param stuffedPListBuffer vector reference that gets stuffed with index information
	 *  @param chunkSizeInMB size in MB to read in from the vector of indexes file
	 *  @return vector<string>* containing all the pattern string data
	 */
	void GetPListArchiveMMAP(vector<vector<PListType>*> &stuffedPListBuffer, double chunkSizeInMB = 0);

	/** @brief End of file?
	 *
	 *  @param stuffedPListBuffer vector reference that gets stuffed with index information
	 *  @param chunkSizeInMB size in MB to read in from the vector of indexes file
	 *  @return bool indicates whether the file is at the end, ie EOF
	 */
	bool IsEndOfFile();

	/** @brief Does the file exist or does it need to be created?
	 *
	 *  @return bool indicates whether the file is not on the hard drive
	 */
	bool Exists();

	/** @brief Close the file handle
	 *
	 *  @return void
	 */
	void CloseArchiveMMAP();

	/** These should eventually not be public */
	string fileName;
	string patternName;
	PListType mappingIndex;
	PListType prevMappingIndex;
	PListType totalWritten;
	static PListType hdSectorSize;
	static PListType totalLoops;
	static PListType writeSize;

private:

	/** Error handling */
	void MappingError(int& fileDescriptor, string fileName);
	void UnMappingError(int& fileDescriptor, string fileName);
	void SeekingError(int& fileDescriptor, string fileName);
	void ExtendingFileError(int& fileDescriptor, string fileName);

	/** Members related to pattern file construction */
	vector<string> stringBuffer;
	PListType fileIndex;
	int fd;
	PListType startingIndex;
	bool endOfFileReached;
	ofstream *outputFile;
	PListType prevListIndex;
	PListType prevStartingIndex;
	PListType *mapper;
	PListType prevFileIndex;
};


