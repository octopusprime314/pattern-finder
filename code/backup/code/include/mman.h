/** @file mman.h
 *  @brief Declaration of POSIX memory mapping functions and types for Win32.
 *
 *  Memory mapping capabilites for windows
 *
 *  @author Unknown
 */

#pragma once
					
#include <sys/types.h>
#include "TypeDefines.h"

#define PROT_NONE       0
#define PROT_READ       1
#define PROT_WRITE      2
#define PROT_EXEC       4

#define MAP_FILE        0
#define MAP_SHARED      1
#define MAP_PRIVATE     2
#define MAP_TYPE        0xf
#define MAP_FIXED       0x10
#define MAP_ANONYMOUS   0x20
#define MAP_ANON        MAP_ANONYMOUS

#define MAP_FAILED      ((void *)-1)

/* Flags for msync. */
#define MS_ASYNC        1
#define MS_SYNC         2
#define MS_INVALIDATE   4

void*   mmap(void *addr, PListType len, int prot, int flags, int fildes, off_t off);
void*   mmap64 (void *addr, PListType len, int prot, int flags, int fd, unsigned long long offset);
int     munmap(void *addr, PListType len);
int     _mprotect(void *addr, PListType len, int prot);
int     msync(void *addr, PListType len, int flags);
int     mlock(const void *addr, PListType len);
int     munlock(const void *addr, PListType len);

