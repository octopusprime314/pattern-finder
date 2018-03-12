/** @file ChunkFactory.h
 *  @brief Generates files with pattern data.
 *
 *  Singleton factory that creates and deletes
 *  files with pattern data.
 *
 *  @author Peter J. Morley (pmorley)
 */

#pragma once
#include "TreeHD.h"
#include <vector>
#include "PListArchive.h"
#include <mutex>
using namespace std;

class ChunkFactory
{
public:
	/** @brief Constructor.
     */
	ChunkFactory();

	/** @brief Destructor.
     */
	~ChunkFactory();

	/** @brief Returns a singleton to be used for pattern file generation
	 *  @return Static ChunkFactory singleton
	 */
	static ChunkFactory* instance()
	{
		static ChunkFactory chunker;
		return &chunker;
	}

	/** @brief Creates a partial pattern file from the PListType vector of vectors
	 *  
	 *  Takes in the new file's name, a vector of vectors containing indexes
	 *  to where patterns can be found within the processed file and level 
	 *  information that includes what size patterns are currently processed,
	 *  the thread processing this data, if this thread is using ram and so on...
	 *
	 *  @param fileName contains only the file name and not the entire address 
	 *  @param leaves contains index locations of patterns
	 *  @param levelInfo contains level related information 
	 *  @return string that contains the entire address of the file's location
	 */
	string CreatePartialPatternFile(string fileName, vector<vector<PListType>*> leaves, LevelPackage levelInfo);

	/** @brief Creates a partial pattern file from the vector containing Hard Drive 
	 *  processing vector information
	 *  
	 *  Takes in the new file's name, a vector of TreeHD objects which essentially
	 *  contain a map of what the pattern is and where it is located within the 
	 *  processed file and level information that includes what size patterns are 
	 *  currently processed, the thread processing this data, if this thread is 
	 *  using ram and so on...
	 *  @param fileName contains only the file name and not the entire address 
	 *  @param leaf contains a map of all pattern data and index locations.  TreeHD
	 *  keeps track of the pattern string so it consumes much more memory.
	 *  @param levelInfo contains level related information 
	 *  @return string that contains the entire address of the file's location
	 */

	string CreatePartialPatternFile(string fileName, vector<TreeHD>& leaf, LevelPackage levelInfo);

	/** @brief Deletes all partial pattern pair files within a folder
	 *  
	 *  Takes in the beginning of the file name pair to be removed and the folder location they are in
	 * 
	 *  @param fileNames contains the beginning of all file pair names to be removed
	 *  @param folderLocation location of files to be removed
	 *  @return Void
	 */

	void DeletePartialPatternFiles(vector<string> fileNames, string folderLocation);

	/** @brief Deletes a single partial pattern pair file within a folder
	 *  
	 *  Takes in the beginning of the file name pairs to be removed and the folder location they are in
	 * 
	 *  @param fileChunkName contains the beginning of a single file pair name to be removed
	 *  @param folderLocation location of file to be removed
	 *  @return Void
	 */

	void DeletePartialPatternFile(string fileChunkName, string folderLocation);

	/** @brief Deletes all complete pattern files within a folder
	 *  
	 *  Takes in the file names to be removed and the folder location they are in
	 * 
	 *  @param fileChunkName contains all the complete pattern file names to be removed
	 *  @param folderLocation location of file to be removed
	 *  @return Void
	 */

	void DeletePatternFiles(vector<string> fileNames, string folderLocation);

	/** @brief Deletes a single partial pattern file within a folder
	 *  
	 *  Takes in a file name to be removed and the folder location they are in
	 * 
	 *  @param fileChunkName contains a single complete pattern file name to be removed
	 *  @param folderLocation location of file to be removed
	 *  @return Void
	 */

	void DeletePatternFile(string fileNames, string folderLocation);

	
	/** @brief Function to generate a unique file id for a partial file
	 *  
	 *  @return PListType unique id
	 */
	PListType GenerateUniqueID()
	{
		PListType id;
		fileIDMutex->lock();
		id = fileID++;
		fileIDMutex->unlock();
		return id;
	}

private:

	/** Unique file id member */
	static PListType fileID;

	/** Mutex for file id generation */
	static mutex* fileIDMutex;

};