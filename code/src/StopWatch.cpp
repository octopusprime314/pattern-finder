#include "StopWatch.h"


StopWatch::StopWatch(void)
{
	begin = std::chrono::high_resolution_clock::now();
}


StopWatch::~StopWatch(void)
{
}

void StopWatch::Start()
{
	begin = std::chrono::high_resolution_clock::now();
}

void StopWatch::Stop()
{
	end = std::chrono::high_resolution_clock::now();
}
double StopWatch::GetTime()
{
	return std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - begin).count();
}
	
void StopWatch::Display()
{
	stringstream buff;
	buff << std::fixed << std::setprecision(2) 
		<< "Wall clock time passed: "
		<< std::chrono::duration<double, std::milli>(end - begin).count()
		<< " ms!\n";
	Logger::WriteLog(buff.str());
	cout << buff.str();
}

void StopWatch::DisplayNow()
{
	stringstream buff;
	buff << std::fixed << std::setprecision(2) 
		<< "Wall clock time passed: "
		<< std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - begin).count()
		<< " ms!\n";
	Logger::WriteLog(buff.str());
	cout << buff.str();
}