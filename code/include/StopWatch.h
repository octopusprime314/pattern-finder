/** @file StopWatch.h
*  @brief StopWatch class gives processing timing information
*
*  @author Peter J. Morley (pmorley)
*/

#pragma once
#include <iomanip>
#include "Logger.h"

class StopWatch
{
public:
    /** @brief Constructor.
    */
    StopWatch(void);

    /** @brief Destructor.
    */
    ~StopWatch(void);

    /** @brief Start the processing timer
    *
    *  @return void
    */
    void Start();

    /** @brief Stop the processing timer
    *
    *  @return void
    */
    void Stop();

    /** @brief Display the processing time using the stop time at the command prompt
    *
    *  @return void
    */
    void Display();

    /** @brief Display the processing time at the current time at the command prompt
    *
    *  @return void
    */
    void DisplayNow();

    /** @brief Get time in milliseconds from start point
    *
    *  @return double time in milliseconds
    */
    double GetTime();

private:
#if defined(_WIN64) || defined(_WIN32)
    std::chrono::steady_clock::time_point begin;
    std::chrono::steady_clock::time_point end;
#elif defined(__linux__)
    std::chrono::system_clock::time_point begin;
    std::chrono::system_clock::time_point end;
#endif
};
