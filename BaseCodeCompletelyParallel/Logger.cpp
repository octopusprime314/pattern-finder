#include "Logger.h"

ofstream* Logger::outputFile = new ofstream(LOGGERPATH + "Log" + Logger::GetFormattedTime() + ".txt", ios_base::in | ios_base::out | ios_base::trunc);
string Logger::stringBuffer;
mutex* Logger::logMutex = new mutex();
int Logger::index;
mutex* Logger::scrollLogMutex = new mutex();
int Logger::verbosity = 0;
//Write to string buffer, if buffer is larger 
void Logger::WriteLog(string miniBuff)
{
	if(verbosity)
	{
		if(!disableLogging )
		{
			logMutex->lock();

			//String gets written live to log while 
			//log.txt gets written to at certain hard disk size blocks
			if(cmdEnabled)
			{
				cout << miniBuff;
			}
//#if defined(_WIN64) || defined(_WIN32) 
//			OutputDebugString(miniBuff.c_str());
//#endif		
			stringBuffer.append(miniBuff);

			//Write to disk only if byte length is greater than 512
			//this is more efficient when writing to disk to do it in blocks
			//if(stringBuffer.length() >= 64)
			//{
				(*outputFile) << stringBuffer;
				//If written clear the string out
				stringBuffer.clear();

				//refresh
				(*outputFile).flush();
				(*outputFile).clear();
			//}

			logMutex->unlock();
		}
	}
}

//Clear string buffer
void Logger::ClearLog()
{
	(*outputFile) << stringBuffer;
	//If written clear the string out
	stringBuffer.clear();
}

//Forces flush all data in string buffer to disk
void Logger::FlushLog()
{

}

//Close log and save time
void Logger::CloseLog()
{
	logMutex->lock();
	ClearLog(); 
	(*outputFile).close();
	logMutex->unlock();

	delete outputFile;
	delete scrollLogMutex;
	delete logMutex;
}


string Logger::GetFormattedTime()
{
	time_t t = time(0);   // get time now
    struct tm * now = new struct tm();
	now = localtime( & t );
	stringstream timeBuff;
	//timeBuff << (now->tm_year + 1900) << '-' << (now->tm_mon + 1) << '-' <<  now->tm_mday << " ";
	string amorpm;

	if(now->tm_hour > 0 && now->tm_hour <= 12)
	{
		now->tm_hour = now->tm_hour;
		amorpm = "am";
	}
	else
	{
		now->tm_hour = now->tm_hour%12;
		amorpm = "pm";
	}

	timeBuff << now->tm_hour << "_";
	if(now->tm_min < 10)
	{
		timeBuff << "0";	
	}
	timeBuff << now->tm_min << "_";
	if(now->tm_sec < 10)
	{
		timeBuff << "0";	
	}
	timeBuff << now->tm_sec /*<< " " << amorpm*/;
	srand ((unsigned int)(time(NULL)));
	timeBuff << rand();

	return timeBuff.str();
}

void Logger::generateTimeVsFileSizeCSV(vector<double> processTimes, vector<PListType> fileSizes)
{
	ofstream csvFile(CSVPATH + "TimeVsFileSize" + Logger::GetFormattedTime() + ".csv" , ios_base::in | ios_base::out | ios_base::trunc);

	if(processTimes.size() == fileSizes.size())
	{
		for(int i = 0; i < processTimes.size(); i++)
		{
			csvFile << processTimes[i] << "," << fileSizes[i] << endl;
		}
	}
	csvFile.close();
}

void Logger::generateFinalPatternVsCount(map<PListType, PListType> finalPattern)
{
	ofstream csvFile(CSVPATH + "FinalPatternVsCount" + Logger::GetFormattedTime() + ".csv" , ios_base::in | ios_base::out | ios_base::trunc);

	for(map<PListType, PListType>::iterator it = finalPattern.begin(); it != finalPattern.end(); it++)
	{
		csvFile << it->first << "," << it->second << "," << endl;
	}
	
	csvFile.close();
}

