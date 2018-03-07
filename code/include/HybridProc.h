#pragma once
#include "SysMemProc.h"
#include "DiskProc.h"
class HybridProc {

	SysMemProc _sysMemProc;
	DiskProc   _diskProc;
public:
	HybridProc();
};