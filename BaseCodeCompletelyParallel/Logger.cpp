#include "Logger.h"

ofstream* Logger::outputFile = new ofstream(LOGGERPATH + "Log" + Logger::GetFormattedTime() + ".txt", ios_base::in | ios_base::out | ios_base::trunc);
string Logger::stringBuffer;
mutex* Logger::logMutex = new mutex();
//string Logger::scroll[SCROLLCOUNT];
int Logger::index;
mutex* Logger::scrollLogMutex = new mutex();
bool Logger::verbosity = false;
//#include "Windows.h"
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


//void Logger::WriteScrollingLog(string subject)
//{
//	scrollLogMutex->lock();
//	if(index < SCROLLCOUNT)
//	{
//		scroll[index] = subject;
//		index++;
//	}
//	else
//	{
//		for(int i = 0; i < SCROLLCOUNT-1; i++)
//		{
//			scroll[i] = scroll[i+1];
//		}
//		scroll[SCROLLCOUNT-1] = subject;
//	}
//	scrollLogMutex->unlock();
//}

//void Logger::DisplayScrollingLog()
//{
//	scrollLogMutex->lock();
//	int scrollStartWidth = glutGet(GLUT_SCREEN_WIDTH);
//	int scrollStartHeight = glutGet(GLUT_SCREEN_HEIGHT);
//	
//	glColor4f(0.0,1.0,0.0,1.0);
//
//	glMatrixMode(GL_PROJECTION);
//	glLoadIdentity();
//	gluOrtho2D(0.0, scrollStartWidth, 0.0, scrollStartHeight);
//
//	glMatrixMode(GL_MODELVIEW);
//	glLoadIdentity();
//
//
//	int positioning = 350;
//	for(unsigned int i = 0; i < SCROLLCOUNT; i++)
//	{
//		glRasterPos2i(0, positioning);
//		for(int j = 0; j < scroll[i].length(); j++)
//		{
//			glutBitmapCharacter(MEDIUM_FONT, scroll[i][j]);
//		}
//		positioning-=12;
//	}
//	scrollLogMutex->unlock();
//}

string Logger::GetFormattedTime()
{
	time_t t = time(0);   // get time now
    struct tm * now = localtime( & t );
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
	srand (time(NULL));
	timeBuff << rand();

	return timeBuff.str();
}


