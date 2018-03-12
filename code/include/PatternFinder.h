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
#include "StopWatch.h"
#include "ChunkFactory.h"

class PatternFinder
{
public:

    /** @brief Constructor.
    */
    PatternFinder(int argc, char **argv);

    /** @brief Destructor.
    */
    ~PatternFinder();

private:

};