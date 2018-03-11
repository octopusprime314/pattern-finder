/** @file Forest.h
*  @brief Contains algorithms to process patterns
*
*  Algorithms that process patterns within a file using
*  either excusively the hard drive or ram but can also
*  manage switching to hard drive or ram processing at any level
*  for speed improvements...this class needs to be refactored
*  to have a base class processor with derived classes called
*  hdprocessor and ramprocessor
*
*  @author Peter J. Morley (pmorley)
*/

#pragma once
#include <iostream>
#include <vector>
#include <map>
#include <future>
#include "TreeHD.h"
#include "FileReader.h"
#include "PListArchive.h"
#include "StopWatch.h"
#include "ChunkFactory.h"
#include "ProcessorConfig.h"
#include "ProcessorStats.h"
#include "SysMemProc.h"
#include "DiskProc.h"

using namespace std;

class Forest
{
public:

    /** @brief Constructor.
    */
    Forest(int argc, char **argv);

    /** @brief Destructor.
    */
    ~Forest();

private:

};