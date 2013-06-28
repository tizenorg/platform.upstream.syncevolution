/*
 *  File:         UI_util.cpp
 *
 *  Author:       Beat Forster
 *
 *  Programming interface between a user application
 *  and the Synthesis SyncML client engine.
 *
 *  Copyright (c) 2007-2009 by Synthesis AG (www.synthesis.ch)
 *
 */


#include "prefix_file.h"
#include  "UI_util.h"
#include "SDK_util.h"
#include "SDK_support.h"

// ---- include DLL functionality ----
#if   defined _WIN32
  #include <windows.h>
  #define  DLL_Suffix ".dll"
#elif defined MACOSX
  #include <dlfcn.h>
  #define  DLL_Suffix ".dylib"
  #define  RFlags (RTLD_NOW + RTLD_GLOBAL)
#elif defined LINUX
  #include <dlfcn.h>
  #define  DLL_Suffix ".so"
  #define  RFlags  RTLD_LAZY
#else
  #define  DLL_Suffix ""
#endif
// -----------------------------------


#define MyMod "UI_util" // debug name



namespace sysync {


static TSyError NotFnd( appPointer aRef, cAppCharP /* aName */ )
{
  TSyError err= LOCERR_OK;

  if (aRef==NULL) {
    err= DB_NotFound;
  //printf( "Not found: '%s' err=%d\n", aName, err );
  } // if

  return err;
} // NotFnd


// Connect to DLL
static TSyError ConnectDLL( cAppCharP aDLLname, appPointer &aDLL )
{
  #if   defined _WIN32
    aDLL= LoadLibrary( aDLLname );
  #elif defined MACOSX || defined LINUX
    aDLL=      dlopen( aDLLname, RFlags );
  //printf( "'%s' %s\n", aDLLname, dlerror() );
  #else
    aDLL= NULL;
  #endif

  return NotFnd( aDLL, aDLLname );
} // ConnectDLL



// Get <aFunc> of <aFuncName> at <aDLL>
static TSyError DLL_Func( appPointer aDLL, cAppCharP aFuncName, appPointer &aFunc )
{
  #if   defined _WIN32
    aFunc= (appPointer)GetProcAddress( (HINSTANCE)aDLL, aFuncName );
  #elif defined MACOSX || defined LINUX
    aFunc= dlsym( aDLL, aFuncName );
  #else
    aFunc= NULL;
  #endif

  return NotFnd( aFunc, aFuncName );
} // DLL_Func



static bool IsLib( cAppCharP name )
{
  int    len= strlen(name);
  return len==0 ||  (name[     0 ]=='[' &&
                     name[ len-1 ]==']'); // empty or embraced with "[" "]"
} // IsLib


// Connect SyncML engine
TSyError UI_Connect( UI_Call_In &aCI, appPointer &aDLL, cAppCharP aEngineName,
                                                        CVersion  aPrgVersion,
                                                        uInt16    aDebugFlags )
{
  // Always search for BOTH names, independently of environment
  cAppCharP SyFName= "SySync_ConnectEngine";
  cAppCharP   FName=        "ConnectEngine";
  string        name= aEngineName;
  TSyError       err= 0;
  bool           dbg= ( aDebugFlags & DBG_PLUGIN_DIRECT )!=0;

  CVersion   engVersion;
  appPointer fFunc;

  typedef TSyError (*GetCEProc)( UI_Call_In* aCI, CVersion *aEngVersion,
                                                  CVersion  aPrgVersion,
                                                  uInt16    aDebugFlags );
  GetCEProc fConnectEngine= NULL;

  do {
    aCI = NULL; // no such structure available at the beginning
    aDLL= NULL;
    if (dbg) printf( "name='%s' err=%d\n", name.c_str(), err );

    if (name.empty()) {
       // not yet fully implemented: Take default settings
                         aCI= new SDK_Interface_Struct;
      InitCallback_Pure( aCI,      DB_Callback_Version );
                         aCI->debugFlags= aDebugFlags;
      break;
    } // if

    if (IsLib( name.c_str() )) {
      #ifdef DBAPI_LINKED
        fConnectEngine= SYSYNC_EXTERNAL(ConnectEngine);
      #endif

      break;
    } // if
                         name+= DLL_Suffix;
        err= ConnectDLL( name.c_str(), aDLL ); // try with suffix first
    if (dbg) printf( "modu='%s' err=%d\n", name.c_str(), err );

    if (err) {
                         name= aEngineName;
        err= ConnectDLL( name.c_str(), aDLL ); // then try directly
    } // if

    if (dbg) printf( "modu='%s' err=%d\n", name.c_str(), err );
    if (err) break;

    cAppCharP              fN= SyFName;
    err=   DLL_Func( aDLL, fN,   fFunc );
    fConnectEngine=   (GetCEProc)fFunc;
    if   (dbg) printf( "func err=%d '%s' %s\n", err, fN, RefStr( (void*)fConnectEngine ).c_str() );

    if (!fConnectEngine) { fN=   FName;
      err= DLL_Func( aDLL, fN,   fFunc );
      fConnectEngine= (GetCEProc)fFunc;
      if (dbg) printf( "func err=%d '%s' %s\n", err, fN, RefStr( (void*)fConnectEngine ).c_str() );
   } // if

  } while (false);

  if    (fConnectEngine)
    err= fConnectEngine( &aCI, &engVersion, aPrgVersion, aDebugFlags );
  if (dbg) printf( "call err=%d\n", err );

//DEBUG_DB     ( aCI, MyMod, "ConnectEngine", "aCB=%08X eng=%08X prg=%08X aDebugFlags=%04X err=%d",
//                                             aCB, engVersion, aPrgVersion, aDebugFlags,  err );
  if    (fConnectEngine && err) return err;
  return NotFnd( aCI, "ConnectEngine" );
} // UI_Connect


TSyError UI_Disconnect( UI_Call_In aCI, appPointer aDLL )
{
  // Always search for BOTH names, independently of environment
  cAppCharP SyFName= "SySync_DisconnectEngine";
  cAppCharP   FName=        "DisconnectEngine";
  TSyError       err= 0;
  appPointer   fFunc;

  typedef TSyError (*GetDEProc)( UI_Call_In aCI );
  GetDEProc fDisconnectEngine= NULL;

  do {
    if (aDLL==NULL) {
      #ifdef DBAPI_LINKED
        fDisconnectEngine= SYSYNC_EXTERNAL(DisconnectEngine);
      #endif

      break;
    } // if

    cAppCharP                 fN= SyFName;
    err=      DLL_Func( aDLL, fN,   fFunc );
    fDisconnectEngine=   (GetDEProc)fFunc;

    if (!fDisconnectEngine) { fN=   FName;
      err=    DLL_Func( aDLL, fN,   fFunc );
      fDisconnectEngine= (GetDEProc)fFunc;
    } // if

  //printf( "func err=%d %08X\n", err, fConnectEngine );
  } while (false);

  if    (fDisconnectEngine)
    err= fDisconnectEngine( aCI );
//printf( "call err=%d\n", err );

//DEBUG_DB     ( aCI, MyMod, "ConnectEngine", "aCB=%08X eng=%08X prg=%08X aDebugFlags=%04X err=%d",
//                                             aCB, engVersion, aPrgVersion, aDebugFlags,  err );
  if (fDisconnectEngine && err) return err;
  return NotFnd( aCI, "DisconnectEngine" );
} // UI_Disconnect



// <uContext> will be casted to the UIContext* structure
UIContext* UiC( CContext uContext ) { return (UIContext*)uContext; }


// Create a UI context
TSyError UI_CreateContext( CContext &uContext, cAppCharP aEngineName,
                           CVersion  aPrgVersion,
                           uInt16    aDebugFlags )
{
  TSyError err;
  UIContext*           uc= new UIContext;
  err=     UI_Connect( uc->uCI, uc->uDLL, aEngineName, aPrgVersion, aDebugFlags );
                       uc->uName=         aEngineName;
  DEBUG_DB           ( uc->uCI, MyMod,"UI_CreateContext", "'%s'", uc->uName.c_str() );
  uContext=  (CContext)uc;
  return err;
} // UI_CreateContext



// Delete a UI context
TSyError UI_DeleteContext( CContext uContext )
{
  UIContext* uc= UiC( uContext );
  DEBUG_DB ( uc->uCI, MyMod,"UI_DeleteContext", "'%s'", uc->uName.c_str() );
  delete     uc;      // delete context
  return LOCERR_OK;
} // UI_DeleteContext


} // namespace sysync
/* eof */
