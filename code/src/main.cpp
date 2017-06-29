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


#include "Forest.h"
#include "MemoryUtils.h"
#include "FileReader.h"
using namespace std;


#if defined(_WIN64) || defined(_WIN32)
	/* Microsoft Windows (32-bit). or 64 bit ------------------------------ */
	#if defined(_DEBUG)
		#define _DEBUG
	#endif
	#define _CRTDBG_MAP_ALLOC
		#include <crtdbg.h>
#endif

int main(int argc, char **argv)
{

	//Makes total files handles at once 2048
#if defined(_WIN64) || defined(_WIN32)
	_setmaxstdio(2048);
#endif

	//Used for memory leak detection if in debug, use the intel profiler for this now
#if defined(_WIN64) || defined(_WIN32) && defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	//_CrtSetBreakAlloc(991);
	//_CrtSetBreakAlloc(31482);
	//_CrtSetBreakAlloc(1242);
#endif

	//Grab program memory at inception of program
	double MemoryUsageAtInception = MemoryUtils::GetProgramMemoryConsumption();

	//Kick of the pattern searching program
	Forest *Ent = new Forest(argc, argv);

	delete Ent;

	//Get back the total memory used to see if there is left over memory in the program
	double threadMemoryConsumptionInMB = MemoryUtils::GetProgramMemoryConsumption();
	stringstream crappy;
	crappy << "Errant memory after processing level " << threadMemoryConsumptionInMB - MemoryUsageAtInception << " in MB!\n";
	Logger::WriteLog(crappy.str());
	cout << crappy.str() << endl;
	crappy.str("");

	Logger::CloseLog();

	//Dump memory leaks if in debug
#if defined(_WIN64) || defined(_WIN32) && defined(_DEBUG)
	_CrtDumpMemoryLeaks();
#endif
	return 0;
}
