#pragma once
#include <ctime>
#include <chrono>
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>
#include "Logger.h"
using namespace std;
class StopWatch
{
public:
	StopWatch(void);
	~StopWatch(void);
	void Start();
	void Stop();
	void Display();
	void DisplayNow();
	double GetTime();
private:
	std::chrono::system_clock::time_point begin;
	std::chrono::system_clock::time_point end;
};

