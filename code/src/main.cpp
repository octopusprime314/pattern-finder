
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
