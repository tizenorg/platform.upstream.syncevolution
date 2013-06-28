/*
 *  File:    platform_DLL.cpp
 *
 *  Author:  Beat Forster (bfo@synthesis.ch)
 *
 *
 *  General interface to access the routines
 *  of a DLL (LINUX platform)
 *
 *  Copyright (c) 2004-2009 by Synthesis AG (www.synthesis.ch)
 *
 *
 */
#define DLOPEN_NO_WARN 1
#define GNU_SOURCE 1
#include <dlfcn.h> // the Linux DLL functionality

#include "platform_DLL.h"
#include <string>
#include <stdio.h>

using namespace std;

/**
 * Instances of this class are returned
 * as DLL handles to the caller of ConnectDLL.
 *
 * Loadable modules are opened dlopen() and
 * functions are found via dlsym(). If the
 * platform supports RTLD_DEFAULT and finds
 * a <DLName>_Module_Version, then dlopen()
 * isn't used and all functions are searched
 * via RTLD_DEFAULT.
 */
class DLWrapper {
  /**
   * name of shared object, including suffix,
   * or function prefix (when functions were
   * linked in)
   */
  string aDLLName;
  /** dlopen() result, NULL if not opened */
  void *aDLL;

public:
  DLWrapper(const char *name) :
    aDLLName(name),
    aDLL(NULL)
  {}

  /** check for linked function or open shared object,
      return true for success */
  bool connect()
  {
#ifdef RTLD_NEXT
    string fullname = aDLLName + "_Module_Version";
    if (dlsym(RTLD_DEFAULT, fullname.c_str()))
      return true;
#endif

    const char* DSuff= ".so";
    string      lName;
    string      aName= aDLLName;

    do {
      aDLL = dlopen(aName.c_str(), RTLD_LAZY); if (!dlerror()) break;      

      aName+= DSuff;
      aDLL = dlopen(aName.c_str(), RTLD_LAZY); if (!dlerror()) break;

      lName= "./";   lName+= aName;    // try Linux current path as well
      aDLL = dlopen( lName.c_str(), RTLD_LAZY ); if (!dlerror()) break;
      aDLL = dlopen( aDLLName.c_str(),      RTLD_LAZY ); if (!dlerror()) break;

      lName= "./";   lName+= aDLLName; // try Linux current path as well
      aDLL = dlopen( lName.c_str(), RTLD_LAZY );
    } while (false);

    return !dlerror();
  }

  bool destroy()
  {
    bool ok  = true;
    if (this) {
      if (aDLL) {
        int err = dlclose(aDLL);
        ok = !err;
      }
      delete this;
    }
    return ok;
  }

  bool function(const char *aFuncName, void *&aFunc)
  {
#ifdef RTLD_DEFAULT
    if (!aDLL) {
      string fullname = aDLLName + '_' + aFuncName;
      aFunc = dlsym(RTLD_DEFAULT, fullname.c_str());
    } else
#endif
      aFunc = dlsym(aDLL, aFuncName);
    return aFunc!=NULL && dlerror()==NULL;
  }
};

bool ConnectDLL( void* &aDLL, const char* aDLLname, ErrReport aReport, void* ref )
/* Connect to <aDLLname>, result is <aDLL> reference */
/* Returns true, if successful */
{
  DLWrapper *wrapper = new DLWrapper(aDLLname);
  bool ok = false;
  if  (wrapper) {
    ok = wrapper->connect();
    if (ok)
      aDLL = (void *)wrapper;
    else
      delete wrapper;
  }

  if   (!ok && aReport) aReport( ref, aDLLname );
  return ok;
} // ConnectDLL



bool DisconnectDLL( void* aDLL )
/* Disconnect <hDLL> */
{
  return ((DLWrapper  *)aDLL)->destroy();
} // DisonnectDLL



bool DLL_Function( void* aDLL, const char* aFuncName, void* &aFunc )
/* Get <aFunc> of <aFuncName> */
/* Returns true, if available */
{
  return ((DLWrapper *)aDLL)->function(aFuncName, aFunc);
} // DLL_Function


/* eof */
