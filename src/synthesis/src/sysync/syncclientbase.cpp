/*
 *  TSyncClientBase
 *    Abstract baseclass for client
 *
 *  Copyright (c) 2002-2009 by Synthesis AG (www.synthesis.ch)
 *
 */

#include "prefix_file.h"

#include "sysync.h"
#include "syncclientbase.h"
#include "syncclient.h"

namespace sysync {

#ifdef ENGINEINTERFACE_SUPPORT

// Support for EngineModule common interface
// =========================================

#ifndef ENGINE_LIBRARY

#warning "using ENGINEINTERFACE_SUPPORT in old-style appbase-rooted environment. Should be converted to real engine usage later"

// Engine factory function for non-Library case
ENGINE_IF_CLASS *newEngine(void)
{
  // For real engine based targets, newEngine must create a target-specific derivate
  // of the engine, which then has a suitable newSyncAppBase() method to create the
  // appBase. For old-style environment, a generic TClientEngineInterface is ok, as this
  // in turn calls the global newSyncAppBase() which then returns the appropriate
  // target specific appBase.
  return new TClientEngineInterface;
} // newEngine

/// @brief returns a new application base.
TSyncAppBase *TClientEngineInterface::newSyncAppBase(void)
{
  // For not really engine based targets, the appbase factory function is
  // a global routine (for real engine targets, it is a true virtual of
  // the engineInterface, implemented in the target's leaf engineInterface derivate.
  // - for now, use the global appBase creator routine
  return sysync::newSyncAppBase(); // use global factory function
} // TClientEngineInterface::newSyncAppBase

#else

// EngineInterface methods
// -----------------------

// progress callback which queues up progress events for later delivery via SessionStep()
static bool progressCallback(
  const TProgressEvent &aEvent,
  void *aContext
)
{
  TClientEngineInterface *clientEngineP = static_cast<TClientEngineInterface *>(aContext);

  // handle some events specially
  if (aEvent.eventtype == pev_nop)
    return true; // just continue
  else if (aEvent.eventtype == pev_suspendcheck)
    return !(clientEngineP->fSuspendRequested);
  else {
    // create engine progress record
    TEngineProgressInfo info;
    info.eventtype = (uInt16)(aEvent.eventtype);
    // - datastore ID, if any
    if (aEvent.datastoreID != NULL)
      info.targetID = (sInt32)(aEvent.datastoreID->fLocalDBTypeID);
    else
      info.targetID = 0;
    // - handle display message event specially
    if (aEvent.eventtype==pev_display100) {
      info.extra1 = 0;
      // extra1 is a pointer to the message text, save it for retrieval via SessionKey
      clientEngineP->fAlertMessage = (cAppCharP)(aEvent.extra);
    }
    else {
      info.extra1 = aEvent.extra;
    }
    info.extra2 = aEvent.extra2;
    info.extra3 = aEvent.extra3;
    // append it at the end of the list
    clientEngineP->fProgressInfoList.push_back(info);
  }
  return !(clientEngineP->fAbortRequested);
} // progressCallback




/// @brief Open a session
/// @param aNewSessionH[out] receives session handle for all session execution calls
/// @param aSelector[in] selector, depending on session type. For multi-profile clients: profile ID to use
/// @param aSessionName[in] a text name/id to identify a session, useage depending on session type.
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TClientEngineInterface::OpenSessionInternal(SessionH &aNewSessionH, uInt32 aSelector, cAppCharP aSessionName)
{
  TSyncClientBase *clientBaseP = static_cast<TSyncClientBase *>(getSyncAppBase());

  // No client session may exist when opening a new one
  if (clientBaseP->fClientSessionP)
    return LOCERR_WRONGUSAGE;
  // check type of session
  if (aSelector == SESSIONSEL_DBAPI_TUNNEL) {
    // initiate a DBAPI tunnel session.
    #ifdef DBAPI_TUNNEL_SUPPORT
    // Create a new session, sessionName selects datastore
    fSessionStatus = clientBaseP->CreateTunnelSession(aSessionName);
    if (fSessionStatus==LOCERR_OK) {
      // return the session pointer as handle
      aNewSessionH=(SessionH)clientBaseP->fClientSessionP;
    }
    #else
    return LOCERR_NOTIMP; // tunnel not implemented
    #endif
  }
  else if ((aSelector & ~SESSIONSEL_PROFILEID_MASK) == SESSIONSEL_CLIENT_AS_CHECK) {
    // special autosync-checking "session"
    #ifdef AUTOSYNC_SUPPORT
    // %%% tbi
    return LOCERR_NOTIMP; // %%% not implemented for now
    #else
    return LOCERR_NOTIMP; // no Autosync in this engine
    #endif
  }
  else {
    #ifdef NON_FULLY_GRANULAR_ENGINE
    // Install callback to catch progress events and deliver them via SessionStep
    // - erase the list of queued progress events
    fProgressInfoList.clear();
    fPendingStepCmd=0; // none pending
    // init the flags which are set by STEPCMD_SUSPEND, STEPCMD_ABORT and STEPCMD_TRANSPFAIL
    fAbortRequested=false;
    fSuspendRequested=false;
    // - install the callback for catching all progress events (including those during session creation)
    clientBaseP->fProgressEventFunc=progressCallback;
    clientBaseP->fProgressEventContext=this; // pass link to myself
    #endif // NON_FULLY_GRANULAR_ENGINE
    // Create a new session
    fSessionStatus = clientBaseP->CreateSession();
    // Pass profile ID
    if (fSessionStatus==LOCERR_OK) {
      clientBaseP->fClientSessionP->SetProfileSelector(aSelector & ~SESSIONSEL_PROFILEID_MASK);
      // return the session pointer as handle
      aNewSessionH=(SessionH)clientBaseP->fClientSessionP;
    }
  }
  // done
  return fSessionStatus;
} // TClientEngineInterface::OpenSessionInternal


/// @brief open session specific runtime parameter/settings key
/// @note key handle obtained with this call must be closed BEFORE SESSION IS CLOSED!
/// @param aNewKeyH[out] receives the opened key's handle on success
/// @param aSessionH[in] session handle obtained with OpenSession
/// @param aMode[in] the open mode
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TClientEngineInterface::OpenSessionKey(SessionH aSessionH, KeyH &aNewKeyH, uInt16 aMode)
{
  // %%% add autosync-check session case here
  // must be current session's handle (for now - however
  // future engines might allow multiple concurrent client sessions)
  if (aSessionH != (SessionH)static_cast<TSyncClientBase *>(getSyncAppBase())->fClientSessionP)
    return LOCERR_WRONGUSAGE; // something wrong with that handle
  // get client session pointer
  TSyncClient *clientSessionP = static_cast<TSyncClient *>((void *)aSessionH);
  // create settings key for the session
  aNewKeyH = (KeyH)clientSessionP->newSessionKey(this);
  // done
  return LOCERR_OK;
} // TClientEngineInterface::OpenSessionKey


/// @brief Close a session
/// @note  It depends on session type if this also destroys the session or if it may persist and can be re-opened.
/// @param aSessionH[in] session handle obtained with OpenSession
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TClientEngineInterface::CloseSession(SessionH aSessionH)
{
  TSyncClientBase *clientBaseP = static_cast<TSyncClientBase *>(getSyncAppBase());

  // %%% add autosync-check session case here
  // Nothing to do if no client session exists or none is requested for closing
  if (!clientBaseP->fClientSessionP || !aSessionH)
    return LOCERR_OK; // nop
  // must be current session's handle
  if (aSessionH != (SessionH)clientBaseP->fClientSessionP)
    return LOCERR_WRONGUSAGE; // something wrong with that handle
  // terminate running session (if any)
  clientBaseP->KillClientSession(LOCERR_USERABORT); // closing while session in progress counts as user abort
  // - remove the callback
  clientBaseP->fProgressEventFunc=NULL;
  clientBaseP->fProgressEventContext=NULL;
  // done
  return LOCERR_OK;
} // TClientEngineInterface::CloseSession


/// @brief Executes next step of the session
/// @param aSessionH[in] session handle obtained with OpenSession
/// @param aStepCmd[in/out] step command (STEPCMD_xxx):
///        - tells caller to send or receive data or end the session etc.
///        - instructs engine to suspend or abort the session etc.
/// @param aInfoP[in] pointer to a TEngineProgressInfo structure, NULL if no progress info needed
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TClientEngineInterface::SessionStep(SessionH aSessionH, uInt16 &aStepCmd, TEngineProgressInfo *aInfoP)
{
  TSyncClientBase *clientBaseP = static_cast<TSyncClientBase *>(getSyncAppBase());

  // %%% add autosync-check session case here
  // must be current session's handle (for now - however
  // future engines might allow multiple concurrent client sessions)
  if (aSessionH != (SessionH)clientBaseP->fClientSessionP)
    return LOCERR_WRONGUSAGE; // something wrong with that handle
  // get client session pointer
  TSyncClient *clientSessionP = static_cast<TSyncClient *>((void *)aSessionH);
  #ifdef NON_FULLY_GRANULAR_ENGINE
  // pre-process setp command and generate pseudo-steps to empty progress event queue
  // preprocess general step codes
  switch (aStepCmd) {
    case STEPCMD_TRANSPFAIL :
      // directly abort
      clientSessionP->AbortSession(LOCERR_TRANSPFAIL,true);
      goto abort;
    case STEPCMD_ABORT :
      // directly abort
      clientSessionP->AbortSession(LOCERR_USERABORT,true);
    abort:
      // also set the flag so subsequent progress events will result the abort status
      fAbortRequested=true;
      aStepCmd = STEPCMD_STEP; // convert to normal step
      goto step;
    case STEPCMD_SUSPEND :
      // directly suspend
      clientSessionP->SuspendSession(LOCERR_USERSUSPEND);
      // also set the flag so subsequent pev_suspendcheck events will result the suspend status
      fSuspendRequested=true;
      aStepCmd = STEPCMD_OK; // this is a out-of-order step, and always just returns STEPCMD_OK.
      fSessionStatus = 0; // ok for now, subsequent steps will perform the actual suspend
      goto done; // no more action for now
    case STEPCMD_STEP :
    step:
    // first just return all queued up progress events
    if (fProgressInfoList.size()>0) {
      // get first element in list
      TEngineProgressInfoList::iterator pos = fProgressInfoList.begin();
      // pass it back to caller if caller is interested
      if (aInfoP) {
        *aInfoP = *pos; // copy progress event
      }
      // delete progress event from list
      fProgressInfoList.erase(pos);
      // that's it for now, engine state does not change, wait for next step
      aStepCmd = STEPCMD_PROGRESS;
      return LOCERR_OK;
    }
    else if (fPendingStepCmd != 0) {
      // now return previously generated step command
      // Note: engine is already in the new state matching fPendingStepCmd
      aStepCmd = fPendingStepCmd;
      fSessionStatus = fPendingStatus;
      fPendingStepCmd=0; // none pending any more
      fPendingStatus=0;
      return fSessionStatus; // return pending status now
    }
    // all progress events are delivered, now we can do the real work
  }
  #endif
  // let client session handle it (if a session exists at all)
  fSessionStatus = clientSessionP->SessionStep(aStepCmd, aInfoP);
  #ifdef NON_FULLY_GRANULAR_ENGINE
  // make sure caller issues STEPCMD_STEP to get all pending progress events
  if (fProgressInfoList.size()>0) {
    // save pending step command for returning later
    fPendingStepCmd = aStepCmd;
    fPendingStatus = fSessionStatus;
    // return request for more steps instead
    aStepCmd = STEPCMD_OK;
    fSessionStatus = LOCERR_OK;
  }
  #endif
done:
  // return step status
  return fSessionStatus;
} // TClientEngineInterface::SessionStep


/// @brief returns the SML instance for a given session handle
///   (internal helper to allow TEngineInterface to provide the access to the SyncML buffer)
InstanceID_t TClientEngineInterface::getSmlInstanceOfSession(SessionH aSessionH)
{
  TSyncClientBase *clientBaseP = static_cast<TSyncClientBase *>(getSyncAppBase());

  // %%% add autosync-check session case here
  // must be current session's handle (for now - however
  // future engines might allow multiple concurrent client sessions)
  if (aSessionH != (SessionH)clientBaseP->fClientSessionP)
    return 0; // something wrong with session handle -> no SML instance
  // get client session pointer
  TSyncClient *clientSessionP = static_cast<TSyncClient *>((void *)aSessionH);
  // return SML instance associated with that session
  return clientSessionP->getSmlWorkspaceID();
} // TClientEngineInterface::getSmlInstanceOfSession

TSyError TClientEngineInterface::debugPuts(cAppCharP aFile, int aLine, cAppCharP aFunction,
                                           int aDbgLevel, cAppCharP aPrefix,
                                           cAppCharP aText)
{
  #if defined(SYDEBUG)
  static_cast<TSyncClientBase *>(getSyncAppBase())->getDbgLogger()->DebugPuts(/* aFile, aLine, aFunction, aPrefix */
                                                                              aDbgLevel, aText);
  return 0;
  #else
  return LOCERR_NOTIMP;
  #endif
}

#endif // ENGINE_LIBRARY

#endif // ENGINEINTERFACE_SUPPORT



// TSyncClientBase
// ===============

#ifdef DIRECT_APPBASE_GLOBALACCESS
// Only for old-style targets that still use a global anchor

TSyncClientBase *getClientBase(void)
{
  return static_cast<TSyncClientBase *>(getSyncAppBase());
} // getClientBase

#endif // DIRECT_APPBASE_GLOBALACCESS


// constructor
TSyncClientBase::TSyncClientBase() :
  TSyncAppBase(),
  fClientSessionP(NULL)
{
  // nop so far
} // TSyncClientBase::TSyncClientBase


// destructor
TSyncClientBase::~TSyncClientBase()
{
  // kill session, if any
  KillClientSession();
  fDeleting=true; // flag deletion to block calling critical (virtual) methods
  // delete client session, if any
  SYSYNC_TRY {
    // %%%%%%%% tdb...
  }
  SYSYNC_CATCH (...)
  SYSYNC_ENDCATCH
} // TSyncClientBase::~TSyncClientBase


// Called from SyncML toolkit when a new SyncML message arrives
// - dispatches to session's StartMessage
Ret_t TSyncClientBase::StartMessage(
  InstanceID_t aSmlWorkspaceID, // SyncML toolkit workspace instance ID
  VoidPtr_t aUserData, // pointer to a TSyncClient descendant
  SmlSyncHdrPtr_t aContentP // SyncML tookit's decoded form of the <SyncHdr> element
) {
  TSyncSession *sessionP = static_cast<TSyncClient *>(aUserData); // the client session
  SYSYNC_TRY {
    // let session handle details of StartMessage callback
    return sessionP->StartMessage(aContentP);
  }
  SYSYNC_CATCH (exception &e)
    return HandleDecodingException(sessionP,"StartMessage",&e);
  SYSYNC_ENDCATCH
  SYSYNC_CATCH (...)
    return HandleDecodingException(sessionP,"StartMessage",NULL);
  SYSYNC_ENDCATCH
} // TSyncClientBase::StartMessage



#ifdef DBAPI_TUNNEL_SUPPORT

// - create a new client DBApi tunnel session
localstatus TSyncClientBase::CreateTunnelSession(cAppCharP aDatastoreName)
{
  localstatus sta;

  // create an ordinary session
  sta = CreateSession();
  if (sta==LOCERR_OK) {
    // determine which datastore to address
    sta = fClientSessionP->InitializeTunnelSession(aDatastoreName);
  }
  // done
  return sta;
} // TSyncClientBase::CreateTunnelSession

#endif // DBAPI_TUNNEL_SUPPORT



// create a new client session
localstatus TSyncClientBase::CreateSession(void)
{
  // remove any eventually existing old session first
  KillClientSession();
  // get config
  //TClientConfig *configP = static_cast<TClientConfig *>(getSyncAppBase()->getRootConfig()->fAgentConfigP);
  // create a new client session of appropriate type
  // - use current time as session ID (only for logging purposes)
  string s;
  LONGLONGTOSTR(s,(long long)(getSystemNowAs(TCTX_UTC)));
  fClientSessionP = static_cast<TClientConfig *>(fConfigP->fAgentConfigP)->CreateClientSession(s.c_str());
  if (!fClientSessionP) return LOCERR_UNDEFINED;
  // check expiry here
  return appEnableStatus();
} // TSyncClientBase::CreateSession


// initialize the (already created) client session and link it with the SML toolkit
localstatus TSyncClientBase::InitializeSession(uInt32 aProfileID, bool aAutoSyncSession)
{
  // session must be created before with CreateSession()
  if (!fClientSessionP)
    return LOCERR_WRONGUSAGE;
  // initialitze session
  return fClientSessionP->InitializeSession(aProfileID, aAutoSyncSession);
}


// create a message into the instance buffer
localstatus TSyncClientBase::generateRequest(bool &aDone)
{
  return getClientSession()->NextMessage(aDone);
} // TSyncClientBase::generateRequest



// clear all unprocessed or unsent data from SML workspace
void TSyncClientBase::clrUnreadSmlBufferdata(void)
{
  InstanceID_t myInstance = getClientSession()->getSmlWorkspaceID();
  MemPtr_t p;
  MemSize_t s;

  smlLockReadBuffer(myInstance,&p,&s);
  smlUnlockReadBuffer(myInstance,s);
} // TSyncClientBase::clrUnreadSmlBufferdata



// process message in the instance buffer
localstatus TSyncClientBase::processAnswer(void)
{
  return fClientSessionP->processAnswer();
}


// - extract hostname from an URI according to transport
void TSyncClientBase::extractHostname(const char *aURI, string &aHostName)
{
  splitURL(aURI,NULL,&aHostName,NULL,NULL,NULL);
} // TSyncClientBase::extractHostname


// - extract document name from an URI according to transport
void TSyncClientBase::extractDocumentInfo(const char *aURI, string &aDocName)
{
  splitURL(aURI,NULL,NULL,&aDocName,NULL,NULL);
} // TSyncClientBase::extractDocumentInfo


// - extract protocol name from an URI according to transport
void TSyncClientBase::extractProtocolname(const char *aURI, string &aProtocolName)
{
  splitURL(aURI,&aProtocolName,NULL,NULL,NULL,NULL);
} // TSyncClientBase::extractProtocolname


// delete and unlink current session from SML toolkit
void TSyncClientBase::KillClientSession(localstatus aStatusCode)
{
  if (fClientSessionP) {
    // Abort session
    if (aStatusCode) fClientSessionP->AbortSession(aStatusCode,true);
    // remove instance of that session
    freeSmlInstance(fClientSessionP->getSmlWorkspaceID());
    fClientSessionP->setSmlWorkspaceID(0); // make sure it isn't set any more
    // delete session itself
    TSyncClient *clientP = fClientSessionP;
    fClientSessionP=NULL;
    delete clientP;
  }
} // TSyncClientBase::KillSession

} // namespace sysync

// eof
