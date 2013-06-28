/*
 *  File:    platform_mutex.h
 *
 *  Author:  Beat Forster (bfo@synthesis.ch)
 *
 *
 *  Mutex handling
 *
 *  Copyright (c) 2005-2009 by Synthesis AG (www.synthesis.ch)
 *
 *
 */

#ifndef PLATFORM_MUTEX_H
#define PLATFORM_MUTEX_H


typedef void* MutexPtr_t;

MutexPtr_t newMutex();
bool      lockMutex( MutexPtr_t m );
bool   tryLockMutex( MutexPtr_t m );
bool    unlockMutex( MutexPtr_t m );
void      freeMutex( MutexPtr_t m );


#endif /* PLATFORM_MUTEX_H */
/* eof */
