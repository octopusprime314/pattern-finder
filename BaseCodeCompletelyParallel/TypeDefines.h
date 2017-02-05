#pragma once
#include <string.h>
#include <sstream>
#include "Logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#define GREATERTHAN4GB 0
//#define INTEGERS 1
#define BYTES 1
using namespace std;

#if GREATERTHAN4GB
typedef unsigned long long PListType;
#else
typedef unsigned long PListType ;
#endif

#if INTEGERS
typedef unsigned long long PatternType;
#endif

#if BYTES
typedef string PatternType;
#endif