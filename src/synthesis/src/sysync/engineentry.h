/**
 *  @File     engineentry.h
 *
 *  @Author   Beat Forster (bfo@synthesis.ch)
 *
 *    Copyright (c) 2007-2009 by Synthesis AG (www.synthesis.ch)
 *
 */

/*
 */

#ifndef ENGINEENTRY_H
#define ENGINEENTRY_H

#include "generic_types.h"
#include "sync_dbapidef.h"

// we need STL strings
#include <string>
using namespace std;


namespace sysync {


ENGINE_ENTRY void     DebugDB          ( void* aCB,    cAppCharP  aParams )   ENTRY_ATTR;
ENGINE_ENTRY void     DebugExotic      ( void* aCB,    cAppCharP  aParams )   ENTRY_ATTR;
ENGINE_ENTRY void     DebugBlock       ( void* aCB,    cAppCharP  aTag,
                                                       cAppCharP  aDesc,
                                                       cAppCharP  aAttrText ) ENTRY_ATTR;
ENGINE_ENTRY void     DebugEndBlock    ( void* aCB,    cAppCharP  aTag )      ENTRY_ATTR;
ENGINE_ENTRY void     DebugEndThread   ( void* aCB )                          ENTRY_ATTR;

// ----------------------------------------------------------------------------------------
ENGINE_ENTRY TSyError SetStringMode    ( void* aCB,       uInt16  aCharSet,
                                                          uInt16  aLineEndMode,    bool aBigEndian ) ENTRY_ATTR;
ENGINE_ENTRY TSyError InitEngineXML    ( void* aCB,    cAppCharP  aConfigXML )                       ENTRY_ATTR;
ENGINE_ENTRY TSyError InitEngineFile   ( void* aCB,    cAppCharP  aConfigFilePath )                  ENTRY_ATTR;
ENGINE_ENTRY TSyError InitEngineCB     ( void* aCB, TXMLConfigReadFunc aReaderFunc, void* aContext ) ENTRY_ATTR;


ENGINE_ENTRY TSyError OpenSession      ( void* aCB,   SessionH *aSessionH, uInt32  aSelector,
                                         cAppCharP  aSessionName )                ENTRY_ATTR;
ENGINE_ENTRY TSyError OpenSessionKey   ( void* aCB,   SessionH aSessionH,
                                         KeyH *aKeyH,     uInt16  aMode    ) ENTRY_ATTR;
ENGINE_ENTRY TSyError SessionStep      ( void* aCB,   SessionH  aSessionH, uInt16 *aStepCmd,
                                         TEngineProgressInfo *aInfoP   ) ENTRY_ATTR;
ENGINE_ENTRY TSyError GetSyncMLBuffer  ( void* aCB,   SessionH aSessionH,   bool  aForSend,
                                         appPointer *aBuffer,  memSize *aBufSize ) ENTRY_ATTR;
ENGINE_ENTRY TSyError RetSyncMLBuffer  ( void* aCB,   SessionH aSessionH,   bool  aForSend,
                                                                            memSize  aRetSize ) ENTRY_ATTR;
ENGINE_ENTRY TSyError ReadSyncMLBuffer ( void* aCB,   SessionH  aSessionH,
                                         appPointer  aBuffer,  memSize  aBufSize,
                                                                            memSize *aValSize ) ENTRY_ATTR;
ENGINE_ENTRY TSyError WriteSyncMLBuffer( void* aCB,   SessionH aSessionH,
                                         appPointer  aBuffer,  memSize  aValSize ) ENTRY_ATTR;
ENGINE_ENTRY TSyError CloseSession     ( void* aCB,   SessionH  aSessionH )                   ENTRY_ATTR;


ENGINE_ENTRY TSyError OpenKeyByPath    ( void* aCB,   KeyH *aKeyH,
                                         KeyH  aParentKeyH, cAppCharP aPath, uInt16 aMode )   ENTRY_ATTR;
ENGINE_ENTRY TSyError OpenSubkey       ( void* aCB,   KeyH *aKeyH,
                                         KeyH aParentKeyH, sInt32  aID,     uInt16 aMode )   ENTRY_ATTR;
ENGINE_ENTRY TSyError DeleteSubkey     ( void* aCB,   KeyH  aParentKeyH, sInt32  aID )                     ENTRY_ATTR;
ENGINE_ENTRY TSyError GetKeyID         ( void* aCB,   KeyH  aKeyH,       sInt32 *aID )                     ENTRY_ATTR;
ENGINE_ENTRY TSyError SetTextMode      ( void* aCB,   KeyH  aKeyH,       uInt16  aCharSet,
                                         uInt16  aLineEndMode,  bool  aBigEndian )              ENTRY_ATTR;
ENGINE_ENTRY TSyError SetTimeMode      ( void* aCB,   KeyH aKeyH,       uInt16  aTimeMode )               ENTRY_ATTR;
ENGINE_ENTRY TSyError CloseKey         ( void* aCB,   KeyH aKeyH )                                        ENTRY_ATTR;

ENGINE_ENTRY TSyError GetValue         ( void* aCB,   KeyH aKeyH, cAppCharP aValName,  uInt16  aValType,
                                         appPointer  aBuffer, memSize aBufSize, memSize *aValSize ) ENTRY_ATTR;
ENGINE_ENTRY TSyError GetValueByID     ( void* aCB,   KeyH aKeyH,    sInt32 aID,       sInt32  arrIndex,
                                                                                              uInt16  aValType,
                                         appPointer  aBuffer, memSize aBufSize, memSize *aValSize ) ENTRY_ATTR;
ENGINE_ENTRY sInt32   GetValueID       ( void* aCB,   KeyH aKeyH, cAppCharP aName )                       ENTRY_ATTR;
ENGINE_ENTRY TSyError SetValue         ( void* aCB,   KeyH aKeyH, cAppCharP aValName,  uInt16  aValType,
                                         cAppPointer  aBuffer, memSize aValSize )                    ENTRY_ATTR;
ENGINE_ENTRY TSyError SetValueByID     ( void* aCB,   KeyH  aKeyH,    sInt32 aID,       sInt32  arrIndex,
                                         uInt16  aValType,
                                         cAppPointer  aBuffer, memSize aValSize )                    ENTRY_ATTR;


// ---- tunnel --------------------------------------------------------------------------------------------------------
ENGINE_ENTRY TSyError StartDataRead    ( CContext ac,  cAppCharP  lastToken, cAppCharP  resumeToken )       ENTRY_ATTR;
ENGINE_ENTRY TSyError ReadNextItem     ( CContext ac,     ItemID  aID,        appCharP *aItemData,
                                                          sInt32 *aStatus,        bool  aFirst )            ENTRY_ATTR;
ENGINE_ENTRY TSyError ReadItem         ( CContext ac,    cItemID  aID,        appCharP *aItemData )         ENTRY_ATTR;
ENGINE_ENTRY TSyError EndDataRead      ( CContext ac )                                                      ENTRY_ATTR;
ENGINE_ENTRY TSyError StartDataWrite   ( CContext ac )                                                      ENTRY_ATTR;
ENGINE_ENTRY TSyError InsertItem       ( CContext ac,  cAppCharP  aItemData,   cItemID  aID )               ENTRY_ATTR;
ENGINE_ENTRY TSyError UpdateItem       ( CContext ac,  cAppCharP  aItemData,   cItemID  aID, ItemID updID ) ENTRY_ATTR;
ENGINE_ENTRY TSyError MoveItem         ( CContext ac,    cItemID  aID,       cAppCharP           newParID ) ENTRY_ATTR;
ENGINE_ENTRY TSyError DeleteItem       ( CContext ac,    cItemID  aID )                                     ENTRY_ATTR;
ENGINE_ENTRY TSyError EndDataWrite     ( CContext ac,       bool  success,    appCharP *newToken )          ENTRY_ATTR;
ENGINE_ENTRY void     DisposeObj       ( CContext ac,      void*  memory )                                  ENTRY_ATTR;

// ---- asKey ----
ENGINE_ENTRY TSyError ReadNextItemAsKey( CContext ac,     ItemID  aID,            KeyH  aItemKey,
                                                          sInt32 *aStatus,        bool  aFirst )            ENTRY_ATTR;
ENGINE_ENTRY TSyError ReadItemAsKey    ( CContext ac,    cItemID  aID,            KeyH  aItemKey )          ENTRY_ATTR;
ENGINE_ENTRY TSyError InsertItemAsKey  ( CContext ac,       KeyH  aItemKey,    cItemID  aID )               ENTRY_ATTR;
ENGINE_ENTRY TSyError UpdateItemAsKey  ( CContext ac,       KeyH  aItemKey,    cItemID  aID, ItemID updID ) ENTRY_ATTR;



// ----------------------------------------------------------------------------------------
ENGINE_ENTRY void CB_Connect( void* aCB ) ENTRY_ATTR;

// engine local helper, used e.g. from PluginDS/Agent
void CB_Connect_KeyAccess   ( void* aCB );


} // namespace sysync
#endif // ENGINEENTRY_H
// eof
