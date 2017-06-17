#include "StopWatch.h"


StopWatch::StopWatch(void)
{
	//Initalize with a begin time when instantiated
	begin = std::chrono::high_resolution_clock::now();
}

StopWatch::~StopWatch(void)
{
}

void StopWatch::Start()
{
	//Get current time and store in begin timing
	begin = std::chrono::high_resolution_clock::now();
}

void StopWatch::Stop()
{
	//Get current time and store in end timing so we can get the difference between begin and end time
	end = std::chrono::high_resolution_clock::now();
}
double StopWatch::GetTime()
{
	//Get the change in time from end to begin in milliseconds
	return std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - begin).count();
}
	
void StopWatch::Display()
{
	//Display the total time passed between end and begin
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
	//Display the now time from the beginning
	stringstream buff;
	buff << std::fixed << std::setprecision(2) 
		<< "Wall clock time passed: "
		<< std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - begin).count()
		<< " ms!\n";
	Logger::WriteLog(buff.str());
	cout << buff.str();
}