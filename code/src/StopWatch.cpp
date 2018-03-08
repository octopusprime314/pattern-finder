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