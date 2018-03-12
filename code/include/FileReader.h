/** @file FileReader.h
 *  @brief File management wrapper
 *
 *  The FileReader class creates a stream of the file data 
 *  and manages it's information
 *
 *  @author Peter J. Morley (pmorley)
 */

#pragma once
#include <future>
#include "TypeDefines.h"

class FileReader
{
private:

public:
	/** @brief Opens a file and records it's size in bytes
	 *  
	 *  Takes in an existing file's name, a reference that will be set to true
	 *  or false indicating whether the file exists and a boolean forcing
	 *  the file stream to be created or not created for later use
	 *
	 *  @param fileName contains the entire address of the file name
	 *  @param isFile reference that will be set if the file exists
	 *  @param openStream indicates whether the file stream is created at this point 
	 */
	FileReader(string fileName, bool &isFile, bool openStream = false);

	/** @brief Destructor.
     */
	~FileReader();

	/** @brief Clears the file stream containing the byte content of the file
	 *  
	 *  Deallocates the file reader stream
	 *
	 *  @return void
	 */
	void DeleteBuffer();

	/** @brief Opens the file stream if not already open
	 *  
	 *  Load bytes into fileString using the stream reader
	 *
	 *  @return void
	 */
	void LoadFile();

	/** Byte stream of the file */
	string fileString;

	/**Size of the file */
	PListType fileStringSize;

	/**Name of the file */
	string fileName;

	/**File stream reader */
	ifstream *copyBuffer;

    //File converter from char to int storage
    static void intToChar(FileReader * file);

    //Test to see if a file exists in a folder directory
    static bool containsFile(std::string directory);
};