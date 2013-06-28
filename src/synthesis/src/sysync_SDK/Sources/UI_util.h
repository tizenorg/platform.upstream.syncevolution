/*
 *  File:         UI_util.h
 *
 *  Author:       Beat Forster
 *
 *  Programming interface between a user application
 *  and the Synthesis SyncML client engine.
 *
 *  Copyright (c) 2007-2009 by Synthesis AG (www.synthesis.ch)
 *
 *
 */

#include <string>
#include "sync_dbapidef.h"


namespace sysync {


class UIContext {
  public:
    UI_Call_In uCI;
    appPointer uDLL;
    string     uName;
}; // UIContext

// <uContext> will be casted to the UIContext* structure
UIContext* UiC( CContext uContext );


/* Function definitions */
TSyError UI_Connect    ( UI_Call_In &aCI, 
                         appPointer &aDLL,     cAppCharP aEngineName,
                                               CVersion  aPrgVersion,
                                               uInt16    aDebugFlags );
TSyError UI_Disconnect ( UI_Call_In  aCI, 
                         appPointer  aDLL );


TSyError UI_CreateContext( CContext &uContext, cAppCharP aEngineName,
                                               CVersion  aPrgVersion,
                                               uInt16    aDebugFlags );
TSyError UI_DeleteContext( CContext  uContext );


} // namespace sysync
/* eof */
