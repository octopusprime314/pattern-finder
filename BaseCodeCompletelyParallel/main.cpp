#if defined(_WIN64) || defined(_WIN32)
	/* Microsoft Windows (32-bit). or 64 bit ------------------------------ */
	#if defined(_DEBUG)
		#define _DEBUG
	#endif
	#define _CRTDBG_MAP_ALLOC
		#include <crtdbg.h>
#endif

#include "Forest.h"
using namespace std;

int main(int argc, char **argv)
{
	
#if defined(_WIN64) || defined(_WIN32) && defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	//_CrtSetBreakAlloc(2945805);
	//_CrtSetBreakAlloc(905187);
	//_CrtSetBreakAlloc(238);
#endif

	Forest *Ent = new Forest(argc, argv);
	delete Ent;

	do
	{
		cout << '\n' <<"Press the Enter key to continue." << endl;
	} while (cin.get() != '\n');


#if defined(_WIN64) || defined(_WIN32) && defined(_DEBUG)
	_CrtDumpMemoryLeaks();
#endif

	return 0;
}
