/*
 *  File:         SyncServer.h
 *
 *  Author:			  Lukas Zeller (luz@synthesis.ch)
 *
 *  TSyncServer
 *    <describe here>
 *
 *  Copyright (c) 2001-2009 by Synthesis AG (www.synthesis.ch)
 *
 *  2001-05-07 : luz : Created
 *
 */

#ifndef SyncServer_H
#define SyncServer_H

// includes
#include "syncsessiondispatch.h"
#include "syncsession.h"
#include "syncdatastore.h"
#include "remotedatastore.h"


using namespace sysync;


namespace sysync {

// Support for SySync Diagnostic Tool
#ifdef SYSYNC_TOOL
int testLogin(int argc, const char *argv[]);
int convertData(int argc, const char *argv[]);
#endif

// forward
class TSyncSessionDispatch;
class TSyncSessionHandle;
class TSyncServer;


// server config
class TServerConfig: public TSessionConfig
{
  typedef TSessionConfig inherited;
public:
  TServerConfig(const char *aElementName, TConfigElement *aParentElementP);
  virtual ~TServerConfig();
  // General server settings
  // - requested auth type (Auth requested in Chal sent to client)
  TAuthTypes fRequestedAuth;
  // - minimally required auth type (lowest auth type that is allowed)
  TAuthTypes fRequiredAuth;
  // - use automatic nonce generation for MD5 auth (empty nonce if false)
  bool fAutoNonce;
  // - constant nonce string to be used if autononce is off. If empty, no nonce is used
  string fConstantNonce;
  // - constant external URL, if set, it is used to generate RespURI (instead of Target LocURI sent by client)
  string fExternalURL;
  // - max size of GUID sent if client does not specify a MaxGUIDSize in devInf. 0=unlimited
  uInt16 fMaxGUIDSizeSent;
protected:
  // check config elements
  #ifndef HARDCODED_CONFIG
  virtual bool localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine);
  #endif
  virtual void clear();
  virtual void localResolve(bool aLastPass);
public:
  // create appropriate session (=agent) for this server
  virtual TSyncServer *CreateServerSession(TSyncSessionHandle *aSessionHandle, const char *aSessionID)=0;
}; // TServerConfig


// server session
class TSyncServer: public TSyncSession
{
  typedef TSyncSession inherited;
public:
  TSyncServer(
    TSyncAppBase *aAppBaseP,
    TSyncSessionHandle *aSessionHandleP,
    const char *aSessionID // a session ID
  );
  virtual ~TSyncServer();
  virtual void ResetSession(void); // like destructor, but without destructing object itself
  void InternalResetSession(void); // static implementation for calling through virtual destructor and virtual ResetSession();
  virtual SmlPcdataPtr_t newResponseURIForRemote(void); // response URI
  // info about session
  virtual bool IsServerSession(void) { return true; }; // is server
  // info about needed auth type
  virtual TAuthTypes requestedAuthType(void);
  virtual bool isAuthTypeAllowed(TAuthTypes aAuthType);
  // Request processing
  // - called when incoming SyncHdr fails to execute
  virtual bool syncHdrFailure(bool aTryAgain);
  // - end of request (to make sure even incomplete SyncML messages get cleaned up properly)
  bool EndRequest(bool &aHasData, string &aRespURI, uInt32 aReqBytes); // returns true if session must be deleted
  // - buffer answer in the session's buffer if transport allows it
  Ret_t bufferAnswer(MemPtr_t aAnswer, MemSize_t aAnswerSize);
  // - get buffered answer from the session's buffer if there is any
  void getBufferedAnswer(MemPtr_t &aAnswer, MemSize_t &aAnswerSize);
  // - get byte statistics
  virtual uInt32 getIncomingBytes(void) { return fIncomingBytes; };
  virtual uInt32 getOutgoingBytes(void) { return fOutgoingBytes; };
  // session handling
  // - get session Handle pointer
  TSyncSessionHandle *getSessionHandle(void) { return fSessionHandleP; }
  // returns remaining time for request processing [seconds]
  virtual sInt32 RemainingRequestTime(void);
  // info about server status
  virtual bool serverBusy(void); // return busy status (set by connection limit or app expiry)
  // Sync processing (command group)
  // - start sync group
  virtual bool processSyncStart(
    SmlSyncPtr_t aSyncP,           // the Sync element
    TStatusCommand &aStatusCommand, // pre-set 200 status, can be modified in case of errors
    bool &aQueueForLater // will be set if command must be queued for later (re-)execution
  );
  #ifdef ENGINEINTERFACE_SUPPORT
  /// @brief Get new session key to access details of this session
  virtual appPointer newSessionKey(TEngineInterface *aEngineInterfaceP);
  #endif
protected:
  // access to config
  TServerConfig *getServerConfig(void);
  // internal processing events
  // - message start and end
  virtual bool MessageStarted(SmlSyncHdrPtr_t aContentP, TStatusCommand &aStatusCommand, bool aBad=false);
  virtual void MessageEnded(bool aIncomingFinal);
  // - request end, called by EndRequest, virtual for descendants
  virtual void RequestEnded(bool &aHasData);
  // - map operation
  virtual bool processMapCommand(
    SmlMapPtr_t aMapCommandP,       // the map command contents
    TStatusCommand &aStatusCommand, // pre-set 200 status, can be modified in case of errors
    bool &aQueueForLater
  );
  // Session level auth
  // - get next nonce string top be sent to remote party for subsequent MD5 auth
  virtual void getNextNonce(const char *aDeviceID, string &aNextNonce);
  // - get nonce string, which is expected to be used by remote party for MD5 auth.
  virtual void getAuthNonce(const char *aDeviceID, string &aAuthNonce);
  // device info (uses defaults for server, override to customize)
  virtual string getDeviceID(void) { return SYSYNC_SERVER_DEVID; }
  virtual string getDeviceType(void) { return SYNCML_SERVER_DEVTYP; }
  // set if map command received in this session
  bool fMapSeen;
  // standard nonce generation (without persistent device info)
  // %%% note: move this to session when we start supporting client auth checking
  string fLastNonce; // last nonce, will be returned at getAuthNonce()
  // busy status
  bool fServerIsBusy;
  // buffered answer
  MemPtr_t fBufferedAnswer;
  MemSize_t fBufferedAnswerSize;
  // data transfer statistics
  uInt32 fIncomingBytes;
  uInt32 fOutgoingBytes;
  // server session handle
  TSyncSessionHandle *fSessionHandleP; // the session "handle" (wrapper, containing server specific locking etc.)
}; // TSyncServer


#ifdef ENGINEINTERFACE_SUPPORT

// Support for EngineModule common interface
// =========================================


// Engine module class
class TServerEngineInterface :
  public TEngineInterface
{
  typedef TEngineInterface inherited;
public:
  // constructor
  TServerEngineInterface() {};

  // appbase factory
  virtual TSyncAppBase *newSyncAppBase(void);

  #ifdef ENGINE_LIBRARY
  #error "%%% tbd: Server version of the session running routines must be implemented"
  // Running a Server Sync Session
  // -----------------------------

  /// @brief Open a session
  /// @param aNewSessionH[out] receives session handle for all session execution calls
  /// @param aSelector[in] selector, depending on session type.
  /// @param aSessionName[in] a text name/id to identify a session, useage depending on session type.
  /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
  virtual TSyError OpenSessionInternal(SessionH &aNewSessionH, uInt32 aSelector, cAppCharP aSessionName);

  /// @brief open session specific runtime parameter/settings key
  /// @note key handle obtained with this call must be closed BEFORE SESSION IS CLOSED!
  /// @param aNewKeyH[out] receives the opened key's handle on success
  /// @param aSessionH[in] session handle obtained with OpenSession
  /// @param aMode[in] the open mode
  /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
  virtual TSyError OpenSessionKey(SessionH aSessionH, KeyH &aNewKeyH, uInt16 aMode);
          
  /// @brief Close a session
  /// @note  It depends on session type if this also destroys the session or if it may persist and can be re-opened.
  /// @param aSessionH[in] session handle obtained with OpenSession
  /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
  virtual TSyError CloseSession(SessionH aSessionH);

  /// @brief Executes sync session or other sync related activity step by step
  /// @param aSessionH[in] session handle obtained with OpenSession
  /// @param aStepCmd[in/out] step command (STEPCMD_xxx):
  ///        - tells caller to send or receive data or end the session etc.
  ///        - instructs engine to suspend or abort the session etc.
  /// @param aInfoP[in] pointer to a TEngineProgressInfo structure, NULL if no progress info needed
  /// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure    
  virtual TSyError SessionStep(SessionH aSessionH, uInt16 &aStepCmd,  TEngineProgressInfo *aInfoP = NULL);
  #endif

}; // TServerEngineInterface


// server runtime parameters
class TServerParamsKey :
  public TSessionKey
{
  typedef TSessionKey inherited;

public:
  TServerParamsKey(TEngineInterface *aEngineInterfaceP, TSyncServer *aServerSessionP);

  virtual ~TServerParamsKey() {};

protected:
  // get table describing the fields in the struct
  virtual const TStructFieldInfo *getFieldsTable(void);
  virtual sInt32 numFields(void);
  // get actual struct base address
  virtual uInt8P getStructAddr(void);
public:
  // the associated server session
  TSyncServer *fServerSessionP;
}; // TServerParamsKey

#endif // ENGINEINTERFACE_SUPPORT


}	// namespace sysync

#endif	// SyncServer_H

// eof

