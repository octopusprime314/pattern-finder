
#include "Forest.h"
#include "MemoryUtils.h"
using namespace std;

#if defined(_WIN64) || defined(_WIN32)
	/* Microsoft Windows (32-bit). or 64 bit ------------------------------ */
	#if defined(_DEBUG)
		#define _DEBUG
		//#include "vld.h"
	#endif
	#define _CRTDBG_MAP_ALLOC
		#include <crtdbg.h>
#endif

int main(int argc, char **argv)
{
	
#if defined(_WIN64) || defined(_WIN32) && defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	//_CrtSetBreakAlloc(5381067);
	//_CrtSetBreakAlloc(3372082);
	//_CrtSetBreakAlloc(3292);
#endif

	double MemoryUsageAtInception = MemoryUtils::GetProgramMemoryConsumption();
	Forest *Ent = new Forest(argc, argv);

	double threadMemoryConsumptionInMB = MemoryUtils::GetProgramMemoryConsumption();
	stringstream crappy;
	crappy << "Errant memory after processing level " << threadMemoryConsumptionInMB - MemoryUsageAtInception << " in MB!\n";
	Logger::WriteLog(crappy.str());

	delete Ent;

	threadMemoryConsumptionInMB = MemoryUtils::GetProgramMemoryConsumption();
	crappy.str("");
	crappy << "Errant memory after processing level " << threadMemoryConsumptionInMB - MemoryUsageAtInception << " in MB!\n";
	Logger::WriteLog(crappy.str());

	do
	{
		cout << '\n' <<"Press the Enter key to continue." << endl;
	} while (cin.get() != '\n');

	Logger::CloseLog();
#if defined(_WIN64) || defined(_WIN32) && defined(_DEBUG)
	_CrtDumpMemoryLeaks();
#endif
	return 0;
}
